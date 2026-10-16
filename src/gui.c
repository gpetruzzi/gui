/* gui 0.1.0 -- independent C implementation of a classic terminal editor.
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gui contributors
 */
#include <ncursesw/curses.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#define CTRL(c) ((c) & 31)
#define HISTORY 64
#define FIELD 1024
#define K_SUP (KEY_MAX + 1)
#define K_SDOWN (KEY_MAX + 2)
#define K_COPY (KEY_MAX + 3)
#define K_PASTE (KEY_MAX + 4)
#define K_SHOME (KEY_MAX + 5)
#define K_SEND (KEY_MAX + 6)
#define K_CTRLLEFT (KEY_MAX + 7)
#define K_CTRLRIGHT (KEY_MAX + 8)
#define K_PASTEBEGIN (KEY_MAX + 9)
#define K_PASTEEND (KEY_MAX + 10)

enum { EDIT = 1, BAR, SELECT, DIALOG, BUTTON, SHADOW, STATUS, FRAME, TITLE };
#define EDIT_TOP 2
#define EDIT_LEFT 1
static const char *menu_names[] = {"File", "Edit", "Search", "View", "Options", "Help"};
static const int menu_x[] = {1, 7, 13, 21, 27, 36};
static const int menu_width[] = {4, 4, 6, 4, 7, 4};
typedef struct { wchar_t *s; size_t n, pos, anchor; } Snapshot;
typedef struct {
    wchar_t *s, *saved, *clip;
    size_t n, pos, anchor, clip_n, top;
    int left, preferred;
    char *path;
    Snapshot undo[HISTORY], redo[HISTORY];
    size_t un, rn;
    wchar_t needle[FIELD], replacement[FIELD];
    bool match_case, whole_word, dragging, crlf, bom;
    char message[256];
} Editor;
static Editor e;
static bool screen_ready;
static short palette_saved[3][4];
static int palette_count;

static void cleanup(void) {
    if (!screen_ready) return;
    printf("\033[?2004l"); fflush(stdout);
    for (int i = 0; i < palette_count; ++i)
        init_color(palette_saved[i][0], palette_saved[i][1], palette_saved[i][2], palette_saved[i][3]);
    endwin(); screen_ready = false;
}

static void die(const char *msg) {
    cleanup();
    fprintf(stderr, "gui: %s\n", msg);
    exit(EXIT_FAILURE);
}
static void *alloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) die("Out of memory.");
    return p;
}
static wchar_t *copy_text(const wchar_t *s, size_t n) {
    if (n > SIZE_MAX / sizeof(wchar_t) - 1) die("File is too large.");
    wchar_t *p = alloc((n + 1) * sizeof(*p));
    if (n) memcpy(p, s, n * sizeof(*p));
    p[n] = 0;
    return p;
}
static char *copy_path(const char *s) {
    char *p = alloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}
static void message(const char *s) { snprintf(e.message, sizeof e.message, "%s", s); }
static bool dirty(void) { return wcscmp(e.s, e.saved) != 0; }
static void clear_history(Snapshot *h, size_t *n) {
    while (*n) free(h[--*n].s);
}
static void push_history(Snapshot *h, size_t *n) {
    if (*n == HISTORY) {
        free(h[0].s);
        memmove(h, h + 1, (HISTORY - 1) * sizeof(*h));
        --*n;
    }
    h[(*n)++] = (Snapshot){copy_text(e.s, e.n), e.n, e.pos, e.anchor};
}
static void checkpoint(void) {
    push_history(e.undo, &e.un);
    clear_history(e.redo, &e.rn);
}
static void undo_redo(bool redo) {
    Snapshot *from = redo ? e.redo : e.undo;
    Snapshot *to = redo ? e.undo : e.redo;
    size_t *fn = redo ? &e.rn : &e.un, *tn = redo ? &e.un : &e.rn;
    if (!*fn) return;
    push_history(to, tn);
    Snapshot s = from[--*fn];
    free(e.s);
    e.s = s.s; e.n = s.n; e.pos = s.pos; e.anchor = s.anchor;
    e.preferred = -1;
}
static void range(size_t *a, size_t *b) {
    *a = e.pos < e.anchor ? e.pos : e.anchor;
    *b = e.pos > e.anchor ? e.pos : e.anchor;
}
/* The caller owns the undo checkpoint. Inserted text must not alias e.s. */
static void splice(size_t a, size_t b, const wchar_t *s, size_t n) {
    if (a > b || b > e.n || n > SIZE_MAX - (e.n - (b - a))) die("Invalid editing range.");
    size_t len = e.n - (b - a) + n;
    wchar_t *p;
    if (len > SIZE_MAX / sizeof(*p) - 1) die("File is too large.");
    p = alloc((len + 1) * sizeof(*p));
    memcpy(p, e.s, a * sizeof(*p));
    if (n) memcpy(p + a, s, n * sizeof(*p));
    memcpy(p + a + n, e.s + b, (e.n - b + 1) * sizeof(*p));
    free(e.s);
    e.s = p; e.n = len; e.pos = e.anchor = a + n; e.preferred = -1;
}
static void insert(const wchar_t *s, size_t n) {
    size_t a, b; range(&a, &b);
    if (!n && a == b) return;
    checkpoint(); splice(a, b, s, n);
}
static void copy_selection(bool cut) {
    size_t a, b; range(&a, &b);
    if (a == b) return;
    free(e.clip); e.clip = copy_text(e.s + a, b - a); e.clip_n = b - a;
    if (cut) { checkpoint(); splice(a, b, L"", 0); }
}
static size_t line_start(size_t p) { while (p && e.s[p - 1] != L'\n') --p; return p; }
static size_t line_end(size_t p) { while (p < e.n && e.s[p] != L'\n') ++p; return p; }
static int cell_width(wchar_t c, int col) {
    if (c == L'\t') return 4 - col % 4;
    int w = wcwidth(c);
    return w < 0 ? 1 : w;
}
static int column(size_t p) {
    int col = 0;
    for (size_t i = line_start(p); i < p; ++i) col += cell_width(e.s[i], col);
    return col;
}
static size_t at_column(size_t start, int col) {
    int x = 0;
    while (start < e.n && e.s[start] != L'\n') {
        int w = cell_width(e.s[start], x);
        if (x + w > col) break;
        x += w; ++start;
    }
    return start;
}
static void move_to(size_t p, bool select, bool vertical) {
    e.pos = p > e.n ? e.n : p;
    if (!select) e.anchor = e.pos;
    if (!vertical) e.preferred = -1;
}
static void vertical(int rows, bool select) {
    if (e.preferred < 0) e.preferred = column(e.pos);
    size_t p = line_start(e.pos);
    while (rows < 0 && p) { p = line_start(p - 1); ++rows; }
    while (rows > 0 && line_end(p) < e.n) { p = line_end(p) + 1; --rows; }
    move_to(at_column(p, e.preferred), select, true);
}
static void text_at(int y, int x, const char *s, int max) {
    if (y >= 0 && y < LINES && x >= 0 && x < COLS && max > 0)
        mvaddnstr(y, x, s, max < COLS - x ? max : COLS - x);
}
static void wide_at(int y, int x, wchar_t ch) {
    if (x < 0 || y < 0 || x >= COLS || y >= LINES) return;
    wchar_t s[2] = {ch, 0};
    mvaddnwstr(y, x, s, 1);
}
static void footer(const char *hint) {
    attrset(COLOR_PAIR(STATUS));
    mvhline(LINES - 1, 0, ' ', COLS);
    text_at(LINES - 1, 0, hint, COLS);
}
static int menu_from_x(int x) {
    for (int i = 5; i > 0; --i) if (x >= menu_x[i]) return i;
    return 0;
}
static void draw(void) {
    int height = LINES - 4, width = COLS - 2;
    if (height < 1 || width < 1) { erase(); refresh(); return; }
    e.top = line_start(e.top > e.n ? e.n : e.top);
    size_t start = line_start(e.pos);
    if (start < e.top) e.top = start;
    size_t p = e.top;
    int row = 0;
    while (p < start && p < e.n) { p = line_end(p) + 1; ++row; }
    while (row >= height && e.top < e.n) { e.top = line_end(e.top) + 1; --row; }
    int col = column(e.pos);
    if (col < e.left) e.left = col;
    if (col >= e.left + width) e.left = col - width + 1;
    bkgd(COLOR_PAIR(EDIT)); erase();
    attrset(COLOR_PAIR(BAR)); mvhline(0, 0, ' ', COLS);
    for (int i = 0; i < 6; ++i) {
        text_at(0, menu_x[i], menu_names[i], COLS - menu_x[i]);
        int accelerator = menu_x[i];
        if (accelerator < COLS) mvchgat(0, accelerator, 1, A_UNDERLINE, BAR, NULL);
    }
    attrset(COLOR_PAIR(FRAME));
    for (int x = 1; x < COLS - 1; ++x) {
        wide_at(1, x, L'─'); wide_at(LINES - 2, x, L'─');
    }
    for (int y = EDIT_TOP; y < LINES - 2; ++y) {
        wide_at(y, 0, L'│'); wide_at(y, COLS - 1, L'│');
    }
    wide_at(1, 0, L'┌'); wide_at(1, COLS - 1, L'┐');
    wide_at(LINES - 2, 0, L'└'); wide_at(LINES - 2, COLS - 1, L'┘');
    attrset(COLOR_PAIR(SHADOW)); mvhline(1, 1, ' ', width);
    char title[1024];
    snprintf(title, sizeof title, " %s%s ", e.path ? e.path : "UNTITLED1", dirty() ? " *" : "");
    int title_len = (int)strlen(title);
    int title_x = title_len < width ? (COLS - title_len) / 2 : 1;
    attrset(COLOR_PAIR(BAR));
    text_at(1, title_x, title, width);
    size_t a, b; range(&a, &b);
    p = e.top;
    for (int y = EDIT_TOP; y < EDIT_TOP + height; ++y) {
        int x = 0;
        while (p < e.n && e.s[p] != L'\n') {
            wchar_t c = e.s[p]; int w = cell_width(c, x);
            bool selected = p >= a && p < b;
            /* No current-line highlight: only an explicit selection changes color. */
            attrset(COLOR_PAIR(selected ? SELECT : EDIT));
            if (c == L'\t') {
                for (int j = 0; j < w; ++j)
                    if (x + j >= e.left && x + j - e.left < width)
                        mvaddch(y, EDIT_LEFT + x + j - e.left, ' ');
            } else if (w > 0 && x >= e.left && x + w <= e.left + width) {
                wide_at(y, EDIT_LEFT + x - e.left, iswprint(c) ? c : L'?');
            } else if (w == 0 && x > e.left && x <= e.left + width) {
                /* Combining marks attach to the preceding displayed character. */
                wchar_t mark[2] = {c, 0}; addnwstr(mark, 1);
            }
            x += w; ++p;
        }
        if (p >= e.n) break;
        if (p >= a && p < b && x >= e.left && x - e.left < width) {
            attrset(COLOR_PAIR(SELECT)); mvaddch(y, EDIT_LEFT + x - e.left, ' ');
        }
        ++p;
    }
    char status[1024];
    if (e.message[0]) snprintf(status, sizeof status, "%s", e.message);
    else snprintf(status, sizeof status, "F1=Help  Enter=Execute  Esc=Cancel  Tab=Next Field");
    footer(status);
    attrset(COLOR_PAIR(EDIT)); move(row + EDIT_TOP, EDIT_LEFT + col - e.left); curs_set(1); refresh();
}
typedef struct { int y, x, h, w; } Box;
static Box box_open(const char *title, int height, int width) {
    draw(); curs_set(0);
    Box b = {(LINES - height) / 2, (COLS - width) / 2, height, width};
    attrset(COLOR_PAIR(SHADOW));
    for (int y = 1; y <= height; ++y) mvhline(b.y + y, b.x + 1, ' ', width);
    attrset(COLOR_PAIR(DIALOG));
    for (int y = 0; y < height; ++y) mvhline(b.y + y, b.x, ' ', width);
    attrset(COLOR_PAIR(TITLE)); mvhline(b.y, b.x, ' ', width);
    text_at(b.y, b.x + (width - (int)strlen(title)) / 2, title, width - 2);
    footer("F1=Help  Enter=Execute  Esc=Cancel  Tab=Next Field");
    mvchgat(LINES - 1, 0, COLS, A_NORMAL, BAR, NULL);
    return b;
}
static void button(Box b, int y, int x, const char *label, bool active) {
    int len = (int)strlen(label);
    attrset(COLOR_PAIR(DIALOG));
    /* Foreground black, parent gray behind half-cell shadow. */
    for (int i = 1; i <= len + 2; ++i) wide_at(b.y + y + 1, b.x + x + i, L'▀');
    wide_at(b.y + y, b.x + x + len + 2, L'▄');
    attrset(COLOR_PAIR(BUTTON));
    wide_at(b.y + y, b.x + x, active ? L'►' : L' ');
    text_at(b.y + y, b.x + x + 1, label, len);
    wide_at(b.y + y, b.x + x + len + 1, active ? L'◄' : L' ');
}
static int key(wint_t *value) { return get_wch(value); }
static bool small_screen(int h, int w) {
    if (LINES >= h + 2 && COLS >= w + 2) return false;
    message("Enlarge the terminal to open this dialog."); return true;
}
static int confirm(const char *title, const char *prompt) {
    if (small_screen(7, 58)) return 2;
    int choice = 0;
    for (;;) {
        Box b = box_open(title, 7, 58);
        attrset(COLOR_PAIR(DIALOG)); text_at(b.y + 2, b.x + 2, prompt, 54);
        button(b, 4, 3, "Save", choice == 0);
        button(b, 4, 20, "Discard", choice == 1);
        button(b, 4, 39, "Cancel", choice == 2);
        refresh(); wint_t c; int t = key(&c);
        if (t == ERR) continue;
        if (t == KEY_CODE_YES && c == KEY_RESIZE) return 2;
        if (c == 27) return 2;
        if (c == '\t' || (t == KEY_CODE_YES && c == KEY_RIGHT)) choice = (choice + 1) % 3;
        else if (t == KEY_CODE_YES && (c == KEY_LEFT || c == KEY_BTAB)) choice = (choice + 2) % 3;
        else if (c == '\n' || c == '\r' || (t == KEY_CODE_YES && c == KEY_ENTER)) return choice;
    }
}
static void field(Box b, int y, int x, int width, const wchar_t *s, size_t pos, bool focus) {
    size_t n = wcslen(s), first = 0;
    int cursor = 0, cols = 0;
    for (size_t i = 0; i < pos; ++i) cursor += cell_width(s[i], cursor);
    while (cursor - cols >= width && first < pos) { cols += cell_width(s[first], cols); ++first; }
    attrset(COLOR_PAIR(DIALOG));
    mvaddch(b.y + y, b.x + x, '[');
    mvhline(b.y + y, b.x + x + 1, '.', width);
    mvaddch(b.y + y, b.x + x + width + 1, ']');
    int at = 0;
    for (size_t i = first; i < n; ++i) {
        int cw = cell_width(s[i], at);
        if (at + cw > width) break;
        if (cw > 0) wide_at(b.y + y, b.x + x + 1 + at, iswprint(s[i]) ? s[i] : L'?');
        at += cw;
    }
    if (focus) { move(b.y + y, b.x + x + 1 + cursor - cols); curs_set(1); }
}
static void field_key(wchar_t *s, size_t *pos, size_t cap, wint_t c, int type) {
    size_t n = wcslen(s);
    if (type == KEY_CODE_YES) {
        if (c == KEY_LEFT && *pos) --*pos;
        else if (c == KEY_RIGHT && *pos < n) ++*pos;
        else if (c == KEY_HOME) *pos = 0;
        else if (c == KEY_END) *pos = n;
        else if (c == KEY_DC && *pos < n) memmove(s + *pos, s + *pos + 1, (n - *pos) * sizeof(*s));
        else if (c == KEY_BACKSPACE && *pos) {
            memmove(s + *pos - 1, s + *pos, (n - *pos + 1) * sizeof(*s)); --*pos;
        }
    } else if (c == 127 || c == 8) {
        if (*pos) { memmove(s + *pos - 1, s + *pos, (n - *pos + 1) * sizeof(*s)); --*pos; }
    } else if (iswprint(c) && n + 1 < cap) {
        memmove(s + *pos + 1, s + *pos, (n - *pos + 1) * sizeof(*s)); s[(*pos)++] = (wchar_t)c;
    }
}
static bool path_prompt(const char *title, char **out) {
    if (small_screen(7, 62)) return false;
    wchar_t s[FIELD] = {0}; size_t p = 0;
    if (*out) {
        size_t n = mbstowcs(s, *out, FIELD - 1);
        if (n != (size_t)-1) { s[n] = 0; p = n; } else s[0] = 0;
    }
    int focus = 0;
    for (;;) {
        Box b = box_open(title, 7, 62);
        attrset(COLOR_PAIR(DIALOG)); text_at(b.y + 2, b.x + 2, "File Name:", 9);
        button(b, 4, 13, "OK", focus == 1); button(b, 4, 38, "Cancel", focus == 2);
        field(b, 2, 12, 45, s, p, focus == 0); refresh();
        wint_t c; int t = key(&c); if (t == ERR) continue;
        if (c == 27 || (t == KEY_CODE_YES && c == KEY_RESIZE)) return false;
        if (c == '\t') focus = (focus + 1) % 3;
        else if (t == KEY_CODE_YES && c == KEY_BTAB) focus = (focus + 2) % 3;
        else if (c == '\n' || c == '\r' || (t == KEY_CODE_YES && c == KEY_ENTER)) {
            if (focus == 2) return false;
            if (!s[0]) continue;
            size_t n = wcstombs(NULL, s, 0);
            if (n == (size_t)-1) { message("Invalid file name for the current locale."); return false; }
            char *path = alloc(n + 1); wcstombs(path, s, n + 1); free(*out); *out = path; return true;
        } else if (!focus) field_key(s, &p, FIELD, c, t);
    }
}
static bool save(void) {
    char *path = e.path ? copy_path(e.path) : NULL;
    if (!path && !path_prompt("Save As", &path)) return false;
    struct stat st; bool exists = lstat(path, &st) == 0;
    if (exists && !S_ISREG(st.st_mode)) { message("Save requires a regular file, not a symbolic link."); free(path); return false; }
    char *temp = alloc(strlen(path) + 16); sprintf(temp, "%s.gui-XXXXXX", path);
    int fd = mkstemp(temp);
    if (fd < 0) { message(strerror(errno)); free(temp); free(path); return false; }
    bool ok = true;
    if (exists && fchmod(fd, st.st_mode & 0777) != 0) ok = false;
    FILE *f = fdopen(fd, "wb");
    if (!f) { close(fd); unlink(temp); message(strerror(errno)); free(temp); free(path); return false; }
    if (e.bom && fwrite("\xef\xbb\xbf", 1, 3, f) != 3) ok = false;
    mbstate_t state = {0}; char bytes[MB_LEN_MAX];
    for (size_t i = 0; i < e.n && ok; ++i) {
        if (e.crlf && e.s[i] == L'\n' && fputc('\r', f) == EOF) ok = false;
        size_t n = wcrtomb(bytes, e.s[i], &state);
        if (n == (size_t)-1 || fwrite(bytes, 1, n, f) != n) ok = false;
    }
    if (fflush(f) != 0 || fsync(fd) != 0) ok = false;
    if (fclose(f) != 0) ok = false;
    if (ok && rename(temp, path) != 0) ok = false;
    if (!ok) { unlink(temp); message("Save failed. The original file was kept."); }
    else {
        free(e.path); e.path = path; path = NULL;
        free(e.saved); e.saved = copy_text(e.s, e.n); message("File saved.");
    }
    free(temp); free(path); return ok;
}
static bool may_discard(void) {
    if (!dirty()) return true;
    int answer = confirm("Unsaved Changes", "Save your changes before continuing?");
    return answer == 1 || (answer == 0 && save());
}
static void reset_document(wchar_t *s, size_t n, char *path, bool crlf, bool bom) {
    free(e.s); free(e.saved); free(e.path);
    e.s = s; e.n = n; e.path = path; e.saved = copy_text(s, n);
    e.pos = e.anchor = e.top = 0; e.left = 0; e.preferred = -1; e.crlf = crlf; e.bom = bom;
    clear_history(e.undo, &e.un); clear_history(e.redo, &e.rn);
}
static bool load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (errno == ENOENT) { reset_document(copy_text(L"", 0), 0, copy_path(path), false, false); return true; }
        message(strerror(errno)); return false;
    }
    size_t n = 0, cap = 4096; char *bytes = alloc(cap);
    for (;;) {
        if (n == cap) {
            if (cap > SIZE_MAX / 2) die("File is too large.");
            cap *= 2; char *p = realloc(bytes, cap); if (!p) die("Out of memory."); bytes = p;
        }
        size_t got = fread(bytes + n, 1, cap - n, f); n += got;
        if (!got) break;
    }
    bool ok = !ferror(f); fclose(f);
    if (n > SIZE_MAX / sizeof(wchar_t) - 1) die("File is too large.");
    wchar_t *text = alloc((n + 1) * sizeof(*text)); size_t len = 0, off = 0;
    bool bom = n >= 3 && memcmp(bytes, "\xef\xbb\xbf", 3) == 0, crlf = false;
    if (bom) off = 3;
    mbstate_t state = {0};
    while (off < n && ok) {
        wchar_t c; size_t used = mbrtowc(&c, bytes + off, n - off, &state);
        if (!used || used == (size_t)-1 || used == (size_t)-2) { ok = false; break; }
        off += used;
        if (c == L'\r' && off < n && bytes[off] == '\n') { crlf = true; continue; }
        text[len++] = c;
    }
    text[len] = 0; free(bytes);
    if (!ok) { free(text); message("Cannot read file: valid UTF-8 text without NUL bytes is required."); return false; }
    reset_document(text, len, copy_path(path), crlf, bom); return true;
}
static bool word(wchar_t c) { return iswalnum(c) || c == L'_'; }
static bool matches(size_t p) {
    size_t n = wcslen(e.needle);
    if (!n || n > e.n - p) return false;
    for (size_t i = 0; i < n; ++i) {
        wchar_t a = e.s[p + i], b = e.needle[i];
        if (!e.match_case) { a = (wchar_t)towlower(a); b = (wchar_t)towlower(b); }
        if (a != b) return false;
    }
    return !e.whole_word || ((!p || !word(e.s[p - 1])) && (p + n == e.n || !word(e.s[p + n])));
}
static bool find_next(size_t from) {
    if (!e.needle[0]) { message("Enter text to find first (Ctrl+B)."); return false; }
    for (size_t i = from; i <= e.n; ++i) if (matches(i)) {
        e.anchor = i; e.pos = i + wcslen(e.needle); e.preferred = -1; message(""); return true;
    }
    for (size_t i = 0; i < from && i <= e.n; ++i) if (matches(i)) {
        e.anchor = i; e.pos = i + wcslen(e.needle); e.preferred = -1; message("Search continued from the beginning."); return true;
    }
    message("Text not found."); return false;
}
static bool replace_one(void) {
    size_t a, b; range(&a, &b);
    if (b - a != wcslen(e.needle) || !matches(a)) {
        if (!find_next(e.pos)) return false;
        range(&a, &b);
    }
    checkpoint(); splice(a, b, e.replacement, wcslen(e.replacement)); return true;
}
static bool replace_all(void) {
    if (!e.needle[0]) return false;
    size_t count = 0, n = wcslen(e.needle), r = wcslen(e.replacement), p = 0;
    while (p <= e.n) {
        if (matches(p)) {
            if (!count) checkpoint();
            splice(p, p + n, e.replacement, r); p += r; ++count;
        } else ++p;
    }
    snprintf(e.message, sizeof e.message, "%zu replacements.", count); return count != 0;
}
static void search_dialog(bool replacing) {
    int height = replacing ? 12 : 10, width = replacing ? 50 : 42;
    if (small_screen(height, width)) return;
    int fields = replacing ? 2 : 1, focus = 0, buttons = replacing ? 4 : 3;
    int field_x = replacing ? 15 : 12, field_width = replacing ? 30 : 26;
    const int search_x[] = {4, 17, 32}, replace_x[] = {2, 10, 23, 40};
    const int *button_x = replacing ? replace_x : search_x;
    const char *search_labels[] = {"  OK  ", "Cancel", "Help"};
    const char *replace_labels[] = {"OK", "All", "Cancel", "Help"};
    const char **button_labels = replacing ? replace_labels : search_labels;
    size_t pos[2] = {wcslen(e.needle), wcslen(e.replacement)};
    bool help = false;
    for (;;) {
        Box b = box_open(replacing ? "Replace" : "Find", height, width);
        int opts = replacing ? 5 : 4, by = height - 3;
        attrset(COLOR_PAIR(DIALOG));
        text_at(b.y + 2, b.x + 2, "Find What:", 12);
        if (replacing) text_at(b.y + 3, b.x + 2, "Replace With:", 13);
        char option[80];
        snprintf(option, sizeof option, "[%c] Match Whole Word Only", e.whole_word ? 'X' : ' ');
        text_at(b.y + opts, b.x + 2, option, width - 4);
        if (focus == fields) mvchgat(b.y + opts, b.x + 2, 3, A_REVERSE, DIALOG, NULL);
        snprintf(option, sizeof option, "[%c] Match Case", e.match_case ? 'X' : ' ');
        text_at(b.y + opts + 1, b.x + 2, option, width - 4);
        if (focus == fields + 1) mvchgat(b.y + opts + 1, b.x + 2, 3, A_REVERSE, DIALOG, NULL);
        if (help) text_at(b.y + opts + 2, b.x + 2, "Tab: Next Field  Space: Toggle", width - 4);
        int button_focus = focus - fields - 2;
        for (int i = 0; i < buttons; ++i)
            button(b, by, button_x[i], button_labels[i], button_focus == i);
        field(b, 2, field_x, field_width, e.needle, pos[0], focus == 0);
        if (replacing) field(b, 3, field_x, field_width, e.replacement, pos[1], focus == 1);
        if (replacing && focus == 0) field(b, 2, field_x, field_width, e.needle, pos[0], true);
        refresh(); wint_t c; int t = key(&c); if (t == ERR) continue;
        if (c == 27 || (t == KEY_CODE_YES && c == KEY_RESIZE)) return;
        int total = fields + 2 + buttons;
        if (c == '\t') { focus = (focus + 1) % total; continue; }
        if (t == KEY_CODE_YES && c == KEY_BTAB) { focus = (focus + total - 1) % total; continue; }
        if (t == KEY_CODE_YES && c == KEY_MOUSE) {
            MEVENT m; if (getmouse(&m) != OK || !(m.bstate & (BUTTON1_CLICKED | BUTTON1_PRESSED))) continue;
            int y = m.y - b.y, x = m.x - b.x;
            if (y == 2) focus = 0;
            else if (replacing && y == 3) focus = 1;
            else if (y == opts) { e.whole_word = !e.whole_word; focus = fields; }
            else if (y == opts + 1) { e.match_case = !e.match_case; focus = fields + 1; }
            else if (y == by) {
                int idx = -1;
                for (int i = 0; i < buttons; ++i)
                    if (x >= button_x[i] && x < button_x[i] + (int)strlen(button_labels[i]) + 2) idx = i;
                if (idx < 0) continue;
                focus = fields + 2 + idx; c = '\n'; t = OK;
            }
        }
        bool enter = c == '\n' || c == '\r' || (t == KEY_CODE_YES && c == KEY_ENTER);
        if ((c == ' ' || enter) && focus == fields) e.whole_word = !e.whole_word;
        else if ((c == ' ' || enter) && focus == fields + 1) e.match_case = !e.match_case;
        else if (enter || (c == ' ' && focus >= fields + 2)) {
            int action = focus < fields ? 0 : focus - fields - 2;
            if (action == buttons - 1) help = !help;
            else if (action == buttons - 2) return;
            else {
                bool ok = replacing ? (action == 1 ? replace_all() : replace_one()) : find_next(e.pos);
                if (ok) return;
                help = true;
            }
        } else if (focus < fields) field_key(focus ? e.replacement : e.needle, &pos[focus], FIELD, c, t);
    }
}
static void about(void) {
    if (small_screen(9, 54)) return;
    Box b = box_open("About gui", 9, 54);
    attrset(COLOR_PAIR(DIALOG));
    text_at(b.y + 2, b.x + 4, "gui 0.1.0 - C edition", 45);
    text_at(b.y + 3, b.x + 4, "Original code. Interface with ncursesw.", 45);
    text_at(b.y + 4, b.x + 4, "A project by gpetruzzi. MIT License.", 45);
    button(b, 6, 21, "OK", true); refresh();
    wint_t c; key(&c);
}
enum { NONE, NEW, OPEN, SAVE, QUIT, UNDO, REDO, COPY, CUT, PASTE, FIND, REPLACE, SELECT_ALL, ABOUT, NEXT, CASE, WHOLE };
static int menu(int which) {
restart:;
    const char *file[] = {"New            Ctrl+N", "Open           Ctrl+O", "Save           Ctrl+S", "Exit           Ctrl+Q"};
    const int fa[] = {NEW, OPEN, SAVE, QUIT};
    const char *edit[] = {"Undo           Ctrl+Z", "Redo           Ctrl+Y", "Copy           Ctrl+C", "Cut            Ctrl+X", "Paste          Ctrl+V", "Find           Ctrl+B", "Replace        Ctrl+R", "Select All     Ctrl+A"};
    const int ea[] = {UNDO, REDO, COPY, CUT, PASTE, FIND, REPLACE, SELECT_ALL};
    const char *search[] = {"Find           Ctrl+B", "Replace        Ctrl+R", "Find Next      F3"};
    const int sa[] = {FIND, REPLACE, NEXT};
    const char *view[] = {"Line Numbers: Off", "Line Highlight: Off"};
    const int va[] = {NONE, NONE};
    const char *options[] = {"Match Case", "Match Whole Word Only"};
    const int oa[] = {CASE, WHOLE};
    const char *help[] = {"About gui"}; const int ha[] = {ABOUT};
    const char **labels = which == 0 ? file : which == 1 ? edit : which == 2 ? search : which == 3 ? view : which == 4 ? options : help;
    const int *actions = which == 0 ? fa : which == 1 ? ea : which == 2 ? sa : which == 3 ? va : which == 4 ? oa : ha;
    int count = which == 0 ? 4 : which == 1 ? 8 : which == 2 ? 3 : which == 5 ? 1 : 2;
    if (small_screen(count + 3, 29)) return NONE;
    int selected = 0, x = menu_x[which];
    if (x + 29 > COLS) x = COLS - 29;
    for (;;) {
        draw(); curs_set(0);
        mvchgat(0, menu_x[which], menu_width[which], A_NORMAL, SELECT, NULL);
        footer("F1=Help  Enter=Execute  Esc=Cancel  Left/Right=Next Menu");
        mvchgat(LINES - 1, 0, COLS, A_NORMAL, BAR, NULL);
        attrset(COLOR_PAIR(SHADOW));
        for (int y = 2; y <= count + 3; ++y) mvhline(y, x + 1, ' ', 28);
        attrset(COLOR_PAIR(DIALOG));
        for (int y = 1; y <= count + 2; ++y) mvhline(y, x, ' ', 28);
        for (int col = 1; col < 27; ++col) { wide_at(1, x + col, L'─'); wide_at(count + 2, x + col, L'─'); }
        wide_at(1, x, L'┌'); wide_at(1, x + 27, L'┐');
        wide_at(count + 2, x, L'└'); wide_at(count + 2, x + 27, L'┘');
        for (int i = 0; i < count; ++i) {
            attrset(COLOR_PAIR(DIALOG));
            wide_at(i + 2, x, L'│'); wide_at(i + 2, x + 27, L'│');
            attrset(COLOR_PAIR(i == selected ? SELECT : DIALOG));
            mvhline(i + 2, x + 1, ' ', 26); text_at(i + 2, x + 2, labels[i], 24);
            if (which == 4 && ((i == 0 && e.match_case) || (i == 1 && e.whole_word)))
                mvaddch(i + 2, x + 26, '*');
        }
        refresh(); wint_t c; int t = key(&c); if (t == ERR) continue;
        if (c == 27 || (t == KEY_CODE_YES && c == KEY_RESIZE)) return NONE;
        if (t == KEY_CODE_YES && c == KEY_UP) selected = (selected + count - 1) % count;
        else if (t == KEY_CODE_YES && c == KEY_DOWN) selected = (selected + 1) % count;
        else if (t == KEY_CODE_YES && c == KEY_LEFT) { which = (which + 5) % 6; goto restart; }
        else if (t == KEY_CODE_YES && c == KEY_RIGHT) { which = (which + 1) % 6; goto restart; }
        else if (t == KEY_CODE_YES && c == KEY_MOUSE) {
            MEVENT m; if (getmouse(&m) == OK && (m.bstate & (BUTTON1_CLICKED | BUTTON1_PRESSED))) {
                if (m.y == 0) { which = menu_from_x(m.x); goto restart; }
                if (m.y >= 2 && m.y <= count + 1 && m.x > x && m.x < x + 27) return actions[m.y - 2];
                return NONE;
            }
        } else if (c == '\n' || c == '\r' || (t == KEY_CODE_YES && c == KEY_ENTER)) return actions[selected];
    }
}
static bool action(int a) {
    if (a == QUIT) return !may_discard();
    if (a == NEW && may_discard()) reset_document(copy_text(L"", 0), 0, NULL, false, false);
    else if (a == OPEN) {
        if (!may_discard()) return true;
        char *path = NULL; if (path_prompt("Open File", &path)) load(path); free(path);
    } else if (a == SAVE) save();
    else if (a == UNDO) undo_redo(false);
    else if (a == REDO) undo_redo(true);
    else if (a == COPY) copy_selection(false);
    else if (a == CUT) copy_selection(true);
    else if (a == PASTE && e.clip_n) insert(e.clip, e.clip_n);
    else if (a == FIND) search_dialog(false);
    else if (a == REPLACE) search_dialog(true);
    else if (a == SELECT_ALL) { e.anchor = 0; e.pos = e.n; }
    else if (a == ABOUT) about();
    else if (a == NEXT) find_next(e.pos);
    else if (a == CASE) { e.match_case = !e.match_case; message(e.match_case ? "Match Case: On." : "Match Case: Off."); }
    else if (a == WHOLE) { e.whole_word = !e.whole_word; message(e.whole_word ? "Match Whole Word Only: On." : "Match Whole Word Only: Off."); }
    return true;
}
static size_t mouse_position(int y, int x) {
    size_t p = e.top;
    for (int i = EDIT_TOP; i < y && p < e.n; ++i) { size_t end = line_end(p); p = end < e.n ? end + 1 : end; }
    return at_column(p, x - EDIT_LEFT + e.left);
}
static void bracketed_paste(void) {
    size_t n = 0, cap = 256; wchar_t *s = alloc(cap * sizeof(*s));
    bool previous_cr = false;
    for (;;) {
        wint_t c; int t = key(&c);
        if (t == ERR) continue;
        if (t == KEY_CODE_YES && c == K_PASTEEND) break;
        if (t == KEY_CODE_YES || !c) continue;
        if (c == L'\n' && previous_cr) { previous_cr = false; continue; }
        previous_cr = c == L'\r';
        if (n == cap) {
            if (cap > SIZE_MAX / sizeof(*s) / 2) die("Pasted text is too large.");
            cap *= 2; wchar_t *p = realloc(s, cap * sizeof(*s)); if (!p) die("Out of memory."); s = p;
        }
        s[n++] = c == L'\r' ? L'\n' : (wchar_t)c;
    }
    insert(s, n); free(s);
}
static bool handle(wint_t c, int type) {
    if (type != KEY_CODE_YES) {
        if (c == CTRL('q')) return action(QUIT);
        if (c == CTRL('n')) return action(NEW);
        if (c == CTRL('o')) return action(OPEN);
        if (c == CTRL('s')) return action(SAVE);
        if (c == CTRL('z')) return action(UNDO);
        if (c == CTRL('y')) return action(REDO);
        if (c == CTRL('c')) return action(COPY);
        if (c == CTRL('x')) return action(CUT);
        if (c == CTRL('v')) return action(PASTE);
        if (c == CTRL('a')) return action(SELECT_ALL);
        if (c == CTRL('b')) return action(FIND);
        if (c == CTRL('r')) return action(REPLACE);
        if (c == 27) {
            wint_t next; timeout(100); int t = key(&next); timeout(-1);
            if (t == OK) {
                next = towlower(next);
                if (next == 'a' || next == 'f') return action(menu(0));
                if (next == 'e') return action(menu(1));
                if (next == 'b' || next == 's') return action(menu(2));
                if (next == 'v') return action(menu(3));
                if (next == 'o') return action(menu(4));
                if (next == 'y' || next == 'h') return action(menu(5));
            }
            e.anchor = e.pos; return true;
        }
        if (c == 127 || c == 8) c = KEY_BACKSPACE;
        else if (c == '\n' || c == '\r') { insert(L"\n", 1); return true; }
        else if (c == '\t') { insert(L"\t", 1); return true; }
        else if (iswprint(c)) { wchar_t s = (wchar_t)c; insert(&s, 1); return true; }
        else return true;
    }
    if (c == KEY_F(1)) return action(ABOUT);
    else if (c == KEY_F(3)) find_next(e.pos);
    else if (c == KEY_F(10)) return action(menu(0));
    else if (c == K_COPY) copy_selection(false);
    else if (c == K_PASTE) return action(PASTE);
    else if (c == K_PASTEBEGIN) bracketed_paste();
    else if (c == KEY_LEFT || c == KEY_SLEFT) move_to(e.pos ? e.pos - 1 : 0, c == KEY_SLEFT, false);
    else if (c == KEY_RIGHT || c == KEY_SRIGHT) move_to(e.pos < e.n ? e.pos + 1 : e.n, c == KEY_SRIGHT, false);
    else if (c == KEY_UP || c == K_SUP || c == KEY_SR) vertical(-1, c != KEY_UP);
    else if (c == KEY_DOWN || c == K_SDOWN || c == KEY_SF) vertical(1, c != KEY_DOWN);
    else if (c == KEY_PPAGE) vertical(-(LINES > 5 ? LINES - 5 : 1), false);
    else if (c == KEY_NPAGE) vertical(LINES > 5 ? LINES - 5 : 1, false);
    else if (c == KEY_HOME || c == K_SHOME) move_to(line_start(e.pos), c == K_SHOME, false);
    else if (c == KEY_END || c == K_SEND) move_to(line_end(e.pos), c == K_SEND, false);
    else if (c == K_CTRLLEFT) {
        size_t p = e.pos; while (p && !word(e.s[p - 1])) --p; while (p && word(e.s[p - 1])) --p; move_to(p, false, false);
    } else if (c == K_CTRLRIGHT) {
        size_t p = e.pos; while (p < e.n && word(e.s[p])) ++p; while (p < e.n && !word(e.s[p])) ++p; move_to(p, false, false);
    } else if (c == KEY_BACKSPACE || c == KEY_DC) {
        size_t a, b; range(&a, &b);
        if (a == b) { if (c == KEY_BACKSPACE && a) --a; else if (c == KEY_DC && b < e.n) ++b; }
        if (a != b) { checkpoint(); splice(a, b, L"", 0); }
    } else if (c == KEY_ENTER) insert(L"\n", 1);
    else if (c == KEY_MOUSE) {
        MEVENT m; if (getmouse(&m) != OK) return true;
        if (m.y == 0 && (m.bstate & (BUTTON1_CLICKED | BUTTON1_PRESSED))) {
            return action(menu(menu_from_x(m.x)));
        }
        if (m.y >= EDIT_TOP && m.y < LINES - 2 && m.x >= EDIT_LEFT && m.x < COLS - 1) {
            size_t p = mouse_position(m.y, m.x);
            if (e.dragging && (m.bstate & REPORT_MOUSE_POSITION)) move_to(p, true, false);
            else if (m.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED)) {
                move_to(p, (m.bstate & BUTTON_SHIFT) != 0, false); e.dragging = true;
            }
            if (m.bstate & BUTTON1_RELEASED) { if (e.dragging) move_to(p, true, false); e.dragging = false; }
            if (m.bstate & BUTTON4_PRESSED) vertical(-3, false);
#ifdef BUTTON5_PRESSED
            if (m.bstate & BUTTON5_PRESSED) vertical(3, false);
#endif
        }
    }
    return true;
}
#ifndef GUI_TEST
int main(int argc, char **argv) {
    if (argc == 2 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "--help"))) {
        puts("gui 0.1.0 (C)\nUsage: gui [file]\nCtrl+B Find | Ctrl+R Replace | Ctrl+S Save | Ctrl+Q Exit"); return 0;
    }
    if (argc > 2) { fputs("Usage: gui [file]\n", stderr); return 1; }
    if (!setlocale(LC_ALL, "" ) || MB_CUR_MAX < 4) die("A UTF-8 locale is required (for example LANG=C.UTF-8).");
    setlocale(LC_MESSAGES, "C");
    e.s = copy_text(L"", 0); e.saved = copy_text(L"", 0); e.preferred = -1;
    if (argc == 2 && !load(argv[1])) die(e.message);
    initscr(); screen_ready = true; atexit(cleanup);
    if (!has_colors()) die("A color terminal is required.");
    raw(); noecho(); keypad(stdscr, TRUE); start_color();
    if (COLOR_PAIRS <= TITLE) die("The terminal does not provide enough color pairs.");
    short white = COLORS >= 16 ? 15 : COLOR_WHITE;
    short blue = COLOR_BLUE, gray = COLOR_WHITE;
    if (can_change_color() && COLORS >= 19) {
        const short values[3][3] = {{0, 0, 667}, {1000, 1000, 1000}, {667, 667, 667}};
        for (int i = 0; i < 3; ++i) {
            short slot = (short)(16 + i), r, g, b;
            if (color_content(slot, &r, &g, &b) == ERR) break;
            palette_saved[palette_count][0] = slot;
            palette_saved[palette_count][1] = r;
            palette_saved[palette_count][2] = g;
            palette_saved[palette_count][3] = b;
            ++palette_count;
            init_color(slot, values[i][0], values[i][1], values[i][2]);
        }
        if (palette_count == 3) { blue = 16; white = 17; gray = 18; }
    }
    init_pair(EDIT, white, blue); init_pair(BAR, COLOR_BLACK, gray);
    init_pair(SELECT, blue, gray); init_pair(DIALOG, COLOR_BLACK, gray);
    init_pair(BUTTON, COLOR_BLACK, white); init_pair(SHADOW, COLOR_BLACK, COLOR_BLACK);
    init_pair(STATUS, COLOR_BLACK, gray); init_pair(FRAME, gray, blue); init_pair(TITLE, COLOR_BLACK, white);
    define_key("\033[1;2A", K_SUP); define_key("\033[1;2B", K_SDOWN);
    define_key("\033[1;2D", KEY_SLEFT); define_key("\033[1;2C", KEY_SRIGHT);
    define_key("\033[2;5~", K_COPY); define_key("\033[2;2~", K_PASTE);
    define_key("\033[1;2H", K_SHOME); define_key("\033[1;2F", K_SEND);
    define_key("\033[1;5D", K_CTRLLEFT); define_key("\033[1;5C", K_CTRLRIGHT);
    define_key("\033[200~", K_PASTEBEGIN); define_key("\033[201~", K_PASTEEND);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL); mouseinterval(0);
    printf("\033[?2004h"); fflush(stdout);
    bool running = true;
    while (running) {
        draw(); wint_t c; int t = key(&c); if (t == ERR) continue;
        e.message[0] = 0; running = handle(c, t);
    }
    cleanup();
    free(e.s); free(e.saved); free(e.path); free(e.clip);
    clear_history(e.undo, &e.un); clear_history(e.redo, &e.rn); return 0;
}
#endif

