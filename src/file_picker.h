/* Classic two-pane file dialog for gui. SPDX-License-Identifier: MIT
 * Included after the editor's dialog and text-field helpers.
 */
#include <dirent.h>
#include <strings.h>

typedef struct { char **names; size_t n, cap; } FileList;
static void list_free(FileList *list) {
    for (size_t i = 0; i < list->n; ++i) free(list->names[i]);
    free(list->names); memset(list, 0, sizeof(*list));
}
static void list_add(FileList *list, const char *name) {
    if (list->n == list->cap) {
        size_t cap = list->cap ? list->cap * 2 : 32;
        if (cap < list->cap || cap > SIZE_MAX / sizeof(*list->names)) die("Too many directory entries.");
        char **p = realloc(list->names, cap * sizeof(*p));
        if (!p) die("Out of memory.");
        list->names = p; list->cap = cap;
    }
    list->names[list->n++] = copy_path(name);
}
static char *path_join(const char *dir, const char *name) {
    if (name[0] == '/') return copy_path(name);
    size_t a = strlen(dir), b = strlen(name);
    if (a > SIZE_MAX - b - 2) die("File name is too long.");
    char *path = alloc(a + b + 2);
    memcpy(path, dir, a);
    if (a && dir[a - 1] != '/') path[a++] = '/';
    memcpy(path + a, name, b + 1); return path;
}
static int name_compare(const void *a, const void *b) {
    int cmp = strcasecmp(*(const char *const *)a, *(const char *const *)b);
    return cmp ? cmp : strcmp(*(const char *const *)a, *(const char *const *)b);
}
/* Scan first, then replace the visible lists; a failed scan loses no state. */
static bool directory_scan(const char *path, FileList *files, FileList *dirs) {
    DIR *dir = opendir(path);
    if (!dir) { message(strerror(errno)); return false; }
    FileList next_files = {0}, next_dirs = {0}; bool ok = true;
    for (;;) {
        errno = 0; struct dirent *entry = readdir(dir);
        if (!entry) { if (errno) ok = false; break; }
        if (!strcmp(entry->d_name, ".")) continue;
        if (!strcmp(path, "/") && !strcmp(entry->d_name, "..")) continue;
        char *full = path_join(path, entry->d_name); struct stat st;
        if (stat(full, &st) == 0) {
            if (S_ISDIR(st.st_mode)) list_add(&next_dirs, entry->d_name);
            else if (S_ISREG(st.st_mode)) list_add(&next_files, entry->d_name);
        }
        free(full);
    }
    closedir(dir);
    if (!ok) { list_free(&next_files); list_free(&next_dirs); message("Cannot read directory."); return false; }
    if (next_files.n > 1) qsort(next_files.names, next_files.n, sizeof(*next_files.names), name_compare);
    if (next_dirs.n > 1) qsort(next_dirs.names, next_dirs.n, sizeof(*next_dirs.names), name_compare);
    list_free(files); list_free(dirs); *files = next_files; *dirs = next_dirs; return true;
}
static bool file_name_set(wchar_t *text, size_t *pos, const char *name) {
    wchar_t next[FIELD]; size_t n = mbstowcs(next, name, FIELD);
    if (n == (size_t)-1 || n >= FIELD) { message("File name is not valid UTF-8 or is too long."); return false; }
    memcpy(text, next, (n + 1) * sizeof(*text)); *pos = n; return true;
}
static void list_draw(Box b, int x, int width, int rows, FileList *list,
                      size_t selected, size_t *scroll, bool focus) {
    if (selected < *scroll) *scroll = selected;
    if (selected >= *scroll + (size_t)rows) *scroll = selected - (size_t)rows + 1;
    int top = b.y + 7, left = b.x + x;
    attrset(COLOR_PAIR(DIALOG));
    for (int col = 1; col < width - 1; ++col) {
        wide_at(top, left + col, L'─'); wide_at(top + rows + 1, left + col, L'─');
    }
    wide_at(top, left, L'┌'); wide_at(top, left + width - 1, L'┐');
    wide_at(top + rows + 1, left, L'└'); wide_at(top + rows + 1, left + width - 1, L'┘');
    for (int row = 0; row < rows; ++row) {
        size_t i = *scroll + (size_t)row;
        attrset(COLOR_PAIR(DIALOG));
        wide_at(top + row + 1, left, L'│'); wide_at(top + row + 1, left + width - 1, L'│');
        attrset(COLOR_PAIR(focus && i == selected ? SELECT : DIALOG));
        mvhline(top + row + 1, left + 1, ' ', width - 3);
        if (i < list->n) text_at(top + row + 1, left + 1, list->names[i], width - 4);
        attrset(COLOR_PAIR(BAR)); mvaddch(top + row + 1, left + width - 2, ' ');
    }
    attrset(COLOR_PAIR(BUTTON));
    wide_at(top + 1, left + width - 2, L'↑');
    wide_at(top + rows, left + width - 2, L'↓');
    if (rows > 2 && list->n > (size_t)rows) {
        int thumb = 1 + (int)((double)selected / (double)(list->n - 1) * (rows - 3));
        attrset(COLOR_PAIR(DIALOG)); wide_at(top + 1 + thumb, left + width - 2, L'▒');
    }
}
static bool path_prompt(const char *title, char **out) {
    if (small_screen(18, 50)) return false;
    int height = LINES - 2 < 24 ? LINES - 2 : 24;
    int width = 50;
    int pane_width = (width - 7) / 2, right = width - pane_width - 2, rows = height - 13;
    char *dir = realpath(".", NULL);
    if (!dir) { message(strerror(errno)); return false; }
    wchar_t name[FIELD] = {0}; size_t pos = 0;
    if (*out) {
        char *initial = path_join(dir, *out), *slash = strrchr(initial, '/');
        if (slash) {
            file_name_set(name, &pos, slash + 1);
            if (slash == initial) slash[1] = 0; else *slash = 0;
            char *parent = realpath(initial, NULL);
            if (parent) { free(dir); dir = parent; }
        }
        free(initial);
    } else if (!strcmp(title, "Save As")) file_name_set(name, &pos, "UNTITLED1");
    FileList files = {0}, dirs = {0};
    if (!directory_scan(dir, &files, &dirs)) { free(dir); return false; }
    size_t selected[2] = {0}, scroll[2] = {0}; int focus = 0; bool accepted = false, help = false;
    const int bx[3] = {4, width / 2 - 5, width - 12};
    const char *labels[] = {"  OK  ", " Cancel ", " Help "};
    for (;;) {
        Box b = box_open(title, height, width);
        attrset(COLOR_PAIR(DIALOG));
        text_at(b.y + 2, b.x + 2, "File Name:", 10);
        text_at(b.y + 4, b.x + 2, dir, width - 4);
        text_at(b.y + 6, b.x + 3, "Existing Files:", pane_width - 2);
        text_at(b.y + 6, b.x + right + 1, "Directories:", pane_width - 2);
        list_draw(b, 2, pane_width, rows, &files, selected[0], &scroll[0], focus == 1);
        list_draw(b, right, pane_width, rows, &dirs, selected[1], &scroll[1], focus == 2);
        for (int i = 0; i < 3; ++i) button(b, height - 3, bx[i], labels[i], focus == i + 3);
        if (help) footer("Tab=Next Field  Arrows=Select  Enter=Open Directory/OK  Esc=Cancel");
        if (help) mvchgat(LINES - 1, 0, COLS, A_NORMAL, BAR, NULL);
        field(b, 2, 13, width - 17, name, pos, focus == 0); refresh();
        wint_t c; int t = key(&c); if (t == ERR) continue;
        if (c == 27 || (t == KEY_CODE_YES && c == KEY_RESIZE)) break;
        if (c == '\t') { focus = (focus + 1) % 6; continue; }
        if (t == KEY_CODE_YES && c == KEY_BTAB) { focus = (focus + 5) % 6; continue; }
        if (t == KEY_CODE_YES && c == KEY_F(1)) { help = !help; continue; }
        if (t == KEY_CODE_YES && c == KEY_MOUSE) {
            MEVENT m; if (getmouse(&m) != OK) continue;
            int y = m.y - b.y, x = m.x - b.x;
            if (!(m.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON4_PRESSED
#ifdef BUTTON5_PRESSED
                              | BUTTON5_PRESSED
#endif
                             ))) continue;
            if (y == 2) { focus = 0; continue; }
            if (y >= 8 && y < 8 + rows) {
                int pane = x >= 2 && x < 2 + pane_width ? 0 : x >= right && x < right + pane_width ? 1 : -1;
                if (pane < 0) continue;
                FileList *list = pane ? &dirs : &files; focus = pane + 1;
                if (!list->n) continue;
                size_t at = scroll[pane] + (size_t)(y - 8);
                int edge = (pane ? right : 2) + pane_width - 2;
                if ((m.bstate & BUTTON4_PRESSED) || (x == edge && y == 8))
                    at = selected[pane] ? selected[pane] - 1 : 0;
#ifdef BUTTON5_PRESSED
                else if (m.bstate & BUTTON5_PRESSED) at = selected[pane] + 1;
#endif
                else if (x == edge && y == 7 + rows) at = selected[pane] + 1;
                if (at >= list->n) at = list->n - 1;
                selected[pane] = at;
                if (!pane) file_name_set(name, &pos, list->names[at]);
                if (!(m.bstate & BUTTON1_DOUBLE_CLICKED)) continue;
                c = '\n'; t = OK;
            } else if (y == height - 3) {
                int hit = -1;
                for (int i = 0; i < 3; ++i)
                    if (x >= bx[i] && x < bx[i] + (int)strlen(labels[i]) + 2) hit = i;
                if (hit < 0) continue;
                focus = hit + 3; c = '\n'; t = OK;
            } else continue;
        }
        bool enter = c == '\n' || c == '\r' || (t == KEY_CODE_YES && c == KEY_ENTER);
        if (focus == 1 || focus == 2) {
            int pane = focus - 1; FileList *list = pane ? &dirs : &files;
            size_t old = selected[pane];
            if (t == KEY_CODE_YES && c == KEY_LEFT) { focus = 1; continue; }
            if (t == KEY_CODE_YES && c == KEY_RIGHT) { focus = 2; continue; }
            if (!list->n) continue;
            if (t == KEY_CODE_YES && c == KEY_UP && selected[pane]) --selected[pane];
            else if (t == KEY_CODE_YES && c == KEY_DOWN && selected[pane] + 1 < list->n) ++selected[pane];
            else if (t == KEY_CODE_YES && c == KEY_HOME) selected[pane] = 0;
            else if (t == KEY_CODE_YES && c == KEY_END) selected[pane] = list->n - 1;
            else if (t == KEY_CODE_YES && c == KEY_NPAGE) {
                size_t next = selected[pane] + (size_t)rows; selected[pane] = next < list->n ? next : list->n - 1;
            } else if (t == KEY_CODE_YES && c == KEY_PPAGE)
                selected[pane] = selected[pane] > (size_t)rows ? selected[pane] - (size_t)rows : 0;
            if (!pane && (old != selected[pane] || enter))
                file_name_set(name, &pos, list->names[selected[pane]]);
            if (!enter) continue;
            if (!pane) { focus = 0; continue; }
            char *joined = path_join(dir, list->names[selected[pane]]), *next = realpath(joined, NULL); free(joined);
            if (next && directory_scan(next, &files, &dirs)) {
                free(dir); dir = next; selected[0] = selected[1] = scroll[0] = scroll[1] = 0;
            } else { if (!next) message(strerror(errno)); free(next); }
            continue;
        }
        if (enter || (c == ' ' && focus >= 3)) {
            if (focus == 4) break;
            if (focus == 5) { help = !help; continue; }
            if (!name[0]) { focus = 0; continue; }
            size_t n = wcstombs(NULL, name, 0);
            if (n == (size_t)-1) { message("File name is not valid UTF-8."); continue; }
            char *bytes = alloc(n + 1); wcstombs(bytes, name, n + 1);
            char *candidate = path_join(dir, bytes); free(bytes); struct stat st;
            if (stat(candidate, &st) == 0 && S_ISDIR(st.st_mode)) {
                char *next = realpath(candidate, NULL); free(candidate);
                if (next && directory_scan(next, &files, &dirs)) {
                    free(dir); dir = next; name[0] = 0; pos = 0;
                    selected[0] = selected[1] = scroll[0] = scroll[1] = 0;
                } else free(next);
                continue;
            }
            free(*out); *out = candidate; accepted = true; break;
        } else if (focus == 0) field_key(name, &pos, FIELD, c, t);
    }
    list_free(&files); list_free(&dirs); free(dir); return accepted;
}
static bool overwrite_prompt(const char *path) {
    if (small_screen(8, 58)) return false;
    int focus = 0;
    for (;;) {
        Box b = box_open("Replace Existing File", 8, 58);
        attrset(COLOR_PAIR(DIALOG)); text_at(b.y + 2, b.x + 2, path, 54);
        text_at(b.y + 3, b.x + 2, "This file exists. Replace it?", 54);
        button(b, 5, 8, "Replace", focus == 0); button(b, 5, 35, "Cancel", focus == 1); refresh();
        wint_t c; int t = key(&c); if (t == ERR) continue;
        if (c == 27 || (t == KEY_CODE_YES && c == KEY_RESIZE)) return false;
        if (c == '\t' || (t == KEY_CODE_YES && (c == KEY_LEFT || c == KEY_RIGHT || c == KEY_BTAB))) focus = !focus;
        else if (c == '\n' || c == '\r' || (t == KEY_CODE_YES && c == KEY_ENTER)) return focus == 0;
    }
}

