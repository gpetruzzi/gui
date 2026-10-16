/* Documents, split views and preferences. SPDX-License-Identifier: MIT */
static void pane_capture(void) {
    panes[active_pane] = (Pane){active_document, e.pos, e.anchor, e.top, e.left};
}
static void document_select(int doc) {
    if (doc < 0 || doc >= MAX_DOCS || !documents[doc].s) return;
    pane_capture();
    if (doc != active_document) {
        wchar_t *clip = e.clip; size_t n = e.clip_n;
        e.clip = NULL; e.clip_n = 0;
        active_document = doc;
        free(e.clip); e.clip = clip; e.clip_n = n;
    }
    e.dragging = false; e.preferred = -1;
    pane_capture(); repaint_needed = true;
}
static void pane_select(int pane) {
    if (!split_enabled || pane == active_pane) return;
    pane_capture(); Pane target = panes[pane], previous = panes[active_pane];
    document_select(target.doc); panes[active_pane] = previous; active_pane = pane;
    e.pos = target.pos > e.n ? e.n : target.pos;
    e.anchor = target.anchor > e.n ? e.n : target.anchor;
    e.top = target.top > e.n ? e.n : target.top; e.left = target.left;
    pane_capture(); repaint_needed = true;
}
static int document_new(void) {
    int slot;
    for (slot = 0; slot < MAX_DOCS && documents[slot].s; ++slot) {}
    if (slot == MAX_DOCS) { message("Nine documents are open. Close one before opening another."); return -1; }
    documents[slot].s = copy_text(L"", 0); documents[slot].saved = copy_text(L"", 0);
    documents[slot].preferred = -1;
    document_select(slot); ++text_revision; return slot;
}
static void document_free(int doc) {
    Editor *d = &documents[doc];
    free(d->s); free(d->saved); free(d->path); free(d->clip);
    clear_history(d->undo, &d->un); clear_history(d->redo, &d->rn);
    memset(d, 0, sizeof *d);
}
static void split_window(void) {
    if (split_enabled) { pane_select(1 - active_pane); return; }
    if (LINES < 12) { message("Enlarge the terminal to split the window."); return; }
    pane_capture(); panes[1] = panes[0]; split_row = LINES / 2;
    split_enabled = true; layout_clamp(); repaint_needed = true;
}
static void close_window(void) {
    if (split_enabled) {
        pane_capture(); Pane keep = panes[1 - active_pane];
        document_select(keep.doc);
        e.pos = keep.pos > e.n ? e.n : keep.pos; e.anchor = keep.anchor > e.n ? e.n : keep.anchor;
        e.top = keep.top > e.n ? e.n : keep.top; e.left = keep.left;
        split_enabled = false; active_pane = 0; pane_capture(); repaint_needed = true;
        return;
    }
    if (!may_discard()) return;
    int old = active_document, next = -1;
    for (int i = 0; i < MAX_DOCS; ++i) if (i != old && documents[i].s) { next = i; break; }
    if (next >= 0) { document_select(next); document_free(old); }
    else reset_document(copy_text(L"", 0), 0, NULL, false, false);
    pane_capture(); repaint_needed = true;
}
static bool workspace_quit(void) {
    for (int i = 0; i < MAX_DOCS; ++i) if (documents[i].s) {
        document_select(i);
        if (!may_discard()) return false;
    }
    return true;
}
static void workspace_open(const char *path, const OpenOptions *options) {
    struct stat candidate;
    bool exists = stat(path, &candidate) == 0;
    for (int i = 0; i < MAX_DOCS; ++i) if (documents[i].s && documents[i].path) {
        struct stat current;
        bool same = exists && stat(documents[i].path, &current) == 0 &&
                    candidate.st_dev == current.st_dev && candidate.st_ino == current.st_ino;
        if (same && documents[i].binary_view == options->binary &&
            documents[i].read_only == (options->read_only || options->binary)) { document_select(i); return; }
    }
    int old = active_document;
    bool reuse = !e.path && e.n == 0 && !dirty();
    if (!reuse && document_new() < 0) return;
    int opened = active_document;
    if (!load_mode(path, options) && !reuse) {
        char error[256]; snprintf(error, sizeof error, "%s", e.message);
        document_select(old); document_free(opened); message(error);
    }
    pane_capture(); repaint_needed = true;
}
static void size_window(void) {
    if (!split_enabled) { message("Split the window first (Ctrl+F6)."); return; }
    int original = split_row;
    for (;;) {
        draw(false); footer("Up/Down or drag divider=Resize  Enter=OK  Esc=Cancel"); refresh();
        wint_t c; int t = key(&c); if (t == ERR) continue;
        if (t == OK && c == 27) { split_row = original; break; }
        if ((t == OK && (c == '\n' || c == '\r')) || (t == KEY_CODE_YES && c == KEY_ENTER)) break;
        if (t == KEY_CODE_YES && c == KEY_RESIZE) break;
        if (t == KEY_CODE_YES && c == KEY_UP) --split_row;
        if (t == KEY_CODE_YES && c == KEY_DOWN) ++split_row;
        if (t == KEY_CODE_YES && c == KEY_MOUSE) {
            MEVENT m;
            if (getmouse(&m) == OK && (m.bstate & (BUTTON1_PRESSED | REPORT_MOUSE_POSITION))) split_row = m.y;
        }
        layout_clamp(); repaint_needed = true;
        if (!split_enabled) break;
    }
    layout_clamp(); repaint_needed = true;
}

enum { THEME_ITEMS = 7, THEME_COLORS = 16 };
typedef struct { unsigned char fg, bg; } ThemeColor;
static ThemeColor theme[THEME_ITEMS];
static const ThemeColor default_theme[THEME_ITEMS] = {
    {15, 1}, {0, 7}, {15, 1}, {0, 7}, {0, 7}, {15, 7}, {8, 7}
};
static const char *theme_names[THEME_ITEMS] = {
    "Normal Text", "Selected Text", "Window Border", "Menubar", "Status Line", "Keyboard Accelerators", "Disabled Items"
};
static const char *color_names[THEME_COLORS] = {
    "Black", "Blue", "Green", "Cyan", "Red", "Magenta", "Brown", "White",
    "Dark Gray", "Light Blue", "Light Green", "Light Cyan", "Light Red", "Light Magenta", "Yellow", "Bright White"
};
static short color_number(unsigned index) {
    static const short ansi[16] = {0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15};
    static const short fixed[16] = {16, 19, 34, 37, 124, 127, 130, 248, 240, 63, 83, 87, 203, 207, 227, 231};
    if (COLORS >= 256) return fixed[index % 16];
    return (short)(ansi[index % 16] % (COLORS >= 16 ? 16 : 8));
}
static void theme_pair(int pair, ThemeColor color) { init_pair((short)pair, color_number(color.fg), color_number(color.bg)); }
static void theme_apply(void) {
    theme_pair(EDIT, theme[0]); theme_pair(SELECT, theme[1]);
    theme_pair(FRAME, theme[2]); theme_pair(TITLE_RULE, theme[2]);
    theme_pair(BAR, theme[3]); theme_pair(STATUS, theme[4]);
    theme_pair(HOTKEY, theme[5]); theme_pair(DISABLED, theme[6]);
    repaint_needed = true;
}
static char *settings_directory(void) {
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (xdg && xdg[0] == '/') return path_join(xdg, "gui");
    if (!home || !home[0]) return NULL;
    char *base = path_join(home, ".config"); char *dir = path_join(base, "gui"); free(base); return dir;
}
static void settings_load(void) {
    tab_stops = 8;
    memcpy(theme, default_theme, sizeof theme);
    char *dir = settings_directory(); if (!dir) return;
    char *path = path_join(dir, "settings"); free(dir);
    FILE *f = fopen(path, "r"); free(path); if (!f) return;
    unsigned version, tabs; ThemeColor next[THEME_ITEMS];
    bool ok = fscanf(f, "gui %u\ntabs %u\n", &version, &tabs) == 2 && version == 1 && tabs >= 1 && tabs <= 32;
    for (int i = 0; i < THEME_ITEMS && ok; ++i) {
        unsigned fg, bg;
        ok = fscanf(f, "%u %u", &fg, &bg) == 2 && fg < THEME_COLORS && bg < THEME_COLORS;
        if (ok) next[i] = (ThemeColor){(unsigned char)fg, (unsigned char)bg};
    }
    fclose(f);
    if (ok) { tab_stops = tabs; memcpy(theme, next, sizeof theme); }
}
static void settings_save(void) {
    char *dir = settings_directory(); if (!dir) { message("Preferences apply for this session; HOME is unavailable."); return; }
    char *parent = copy_path(dir), *slash = strrchr(parent, '/');
    if (slash) { *slash = 0; if (mkdir(parent, 0700) && errno != EEXIST) { free(parent); free(dir); message("Cannot create preferences directory."); return; } }
    free(parent);
    if (mkdir(dir, 0700) && errno != EEXIST) { free(dir); message("Cannot create preferences directory."); return; }
    char *path = path_join(dir, "settings"), *temp = path_join(dir, "settings-XXXXXX"); free(dir);
    int fd = mkstemp(temp); bool ok = fd >= 0;
    FILE *f = ok ? fdopen(fd, "w") : NULL;
    if (f) {
        if (fprintf(f, "gui 1\ntabs %u\n", tab_stops) < 0) ok = false;
        for (int i = 0; i < THEME_ITEMS; ++i) if (fprintf(f, "%u %u\n", theme[i].fg, theme[i].bg) < 0) ok = false;
        if (fflush(f)) ok = false;
        if (fsync(fd)) ok = false;
        if (fclose(f)) ok = false;
        if (ok && rename(temp, path)) ok = false;
    } else { if (fd >= 0) close(fd); ok = false; }
    if (!ok) { unlink(temp); message("Preferences apply now, but could not be saved."); }
    free(path); free(temp);
}
static void settings_dialog(void) {
    if (small_screen(9, 48)) return;
    wchar_t value[8]; swprintf(value, 8, L"%u", tab_stops); size_t pos = wcslen(value);
    int focus = 0; bool help = false;
    for (;;) {
        Box b = box_open("Settings", 9, 48);
        hot_label(b.y + 2, b.x + 4, "Tab Stops:", 10, 0);
        field(b, 2, 17, 7, value, pos, false);
        if (help) { attrset(COLOR_PAIR(DIALOG)); text_at(b.y + 4, b.x + 3, "Tab spacing: 1 to 32 columns.", 42); }
        button(b, 6, 3, "  OK  ", focus == 1); button(b, 6, 18, "Cancel", focus == 2); button(b, 6, 34, "Help", focus == 3);
        if (!focus) field(b, 2, 17, 7, value, pos, true);
        refresh(); wint_t c; int t = dialog_key(&c); if (t == ERR) continue;
        if (t == ALT_INPUT) {
            if (c == 't') { focus = 0; continue; }
            if (c == 'c') return;
            if (c == 'h') { help = !help; continue; }
            if (c != 'o') continue;
            focus = 1; c = '\n'; t = OK;
        }
        if ((t == OK && c == 27) || (t == KEY_CODE_YES && c == KEY_RESIZE)) return;
        if (t == OK && c == '\t') { focus = (focus + 1) % 4; continue; }
        if (t == KEY_CODE_YES && c == KEY_BTAB) { focus = (focus + 3) % 4; continue; }
        if (t == KEY_CODE_YES && c == KEY_F(1)) { help = !help; continue; }
        if (t == KEY_CODE_YES && c == KEY_MOUSE) {
            MEVENT m; if (getmouse(&m) != OK || !(m.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED))) continue;
            if (m.y == b.y + 2) { focus = 0; continue; }
            if (m.y != b.y + 6) continue;
            if (m.x >= b.x + 3 && m.x < b.x + 11) focus = 1;
            else if (m.x >= b.x + 18 && m.x < b.x + 26) focus = 2;
            else if (m.x >= b.x + 34 && m.x < b.x + 40) focus = 3;
            else continue;
            c = '\n'; t = OK;
        }
        bool enter = (t == OK && (c == '\n' || c == '\r')) || (t == KEY_CODE_YES && c == KEY_ENTER);
        if (enter || (t == OK && c == ' ' && focus)) {
            if (focus == 2) return;
            if (focus == 3) { help = !help; continue; }
            wchar_t *end; unsigned long tabs = wcstoul(value, &end, 10);
            if (!value[0] || *end || tabs < 1 || tabs > 32) { help = true; focus = 0; continue; }
            tab_stops = (unsigned)tabs; e.preferred = -1; ++text_revision; repaint_needed = true; settings_save(); return;
        }
        if (!focus && (t == KEY_CODE_YES || c == 8 || c == 127 || (c >= '0' && c <= '9')))
            field_key(value, &pos, 4, c, t);
    }
}
static void colors_dialog(void) {
    if (small_screen(17, 68)) return;
    ThemeColor draft[THEME_ITEMS]; memcpy(draft, theme, sizeof draft);
    char *items[THEME_ITEMS], *names[THEME_COLORS];
    for (int i = 0; i < THEME_ITEMS; ++i) items[i] = (char *)theme_names[i];
    for (int i = 0; i < THEME_COLORS; ++i) names[i] = (char *)color_names[i];
    FileList item_list = {0}, color_list = {0};
    item_list.names = items; item_list.n = THEME_ITEMS; color_list.names = names; color_list.n = THEME_COLORS;
    int focus = 0, item = 0; size_t scroll[3] = {0}; bool help = false;
    const int xs[3] = {2, 30, 48}, widths[3] = {26, 16, 18}, bx[4] = {3, 21, 36, 53};
    const char *buttons[4] = {"Default", "  OK  ", "Cancel", "Help"};
    for (;;) {
        Box b = box_open("Colors", 17, 68);
        hot_label(b.y + 2, b.x + xs[0], "Item:", 24, 0);
        hot_label(b.y + 2, b.x + xs[1], "Foreground:", 16, 0);
        hot_label(b.y + 2, b.x + xs[2], "Background:", 17, 0);
        list_draw(b, 3, xs[0], widths[0], 7, &item_list, (size_t)item, &scroll[0], true);
        list_draw(b, 3, xs[1], widths[1], 7, &color_list, draft[item].fg, &scroll[1], true);
        list_draw(b, 3, xs[2], widths[2], 7, &color_list, draft[item].bg, &scroll[2], true);
        if (focus < 3) {
            attrset(COLOR_PAIR(HOTKEY) | A_BOLD | A_UNDERLINE);
            mvaddch(b.y + 2, b.x + xs[focus], focus == 0 ? 'I' : focus == 1 ? 'F' : 'B');
        }
        theme_pair(PREVIEW, draft[item]); attrset(COLOR_PAIR(PREVIEW));
        mvhline(b.y + 12, b.x + 5, ' ', 58); text_at(b.y + 12, b.x + 7, "Sample text: The quick brown fox 0123456789", 54);
        if (help) { attrset(COLOR_PAIR(DIALOG)); text_at(b.y + 13, b.x + 3, "Choose an item, then its text and background colors.", 62); }
        for (int i = 0; i < 4; ++i) button(b, 14, bx[i], buttons[i], focus == i + 3);
        refresh(); wint_t c; int t = dialog_key(&c); if (t == ERR) continue;
        if (t == ALT_INPUT) {
            if (c == 'i' || c == 'f' || c == 'b') { focus = c == 'i' ? 0 : c == 'f' ? 1 : 2; continue; }
            if (c == 'c') return;
            if (c == 'h') { help = !help; continue; }
            if (c != 'd' && c != 'o') continue;
            focus = c == 'd' ? 3 : 4; c = '\n'; t = OK;
        }
        if ((t == OK && c == 27) || (t == KEY_CODE_YES && c == KEY_RESIZE)) return;
        if (t == OK && c == '\t') { focus = (focus + 1) % 7; continue; }
        if (t == KEY_CODE_YES && c == KEY_BTAB) { focus = (focus + 6) % 7; continue; }
        if (t == KEY_CODE_YES && c == KEY_F(1)) { help = !help; continue; }
        if (t == KEY_CODE_YES && c == KEY_MOUSE) {
            MEVENT m; if (getmouse(&m) != OK || !(m.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON4_PRESSED
#ifdef BUTTON5_PRESSED
                | BUTTON5_PRESSED
#endif
                ))) continue;
            int y = m.y - b.y, x = m.x - b.x;
            if (y == 14) {
                int hit = -1;
                for (int i = 0; i < 4; ++i) if (x >= bx[i] && x < bx[i] + (int)strlen(buttons[i]) + 2) hit = i + 3;
                if (hit < 0) continue;
                focus = hit; c = '\n'; t = OK;
            } else {
                int pane = -1;
                for (int i = 0; i < 3; ++i) if (x > xs[i] && x < xs[i] + widths[i] - 1) pane = i;
                if (pane < 0 || y < 4 || y > 10) continue;
                focus = pane; int count = pane ? THEME_COLORS : THEME_ITEMS;
                int at = (int)scroll[pane] + y - 4;
                bool up = (m.bstate & BUTTON4_PRESSED) || (x == xs[pane] + widths[pane] - 2 && y == 4);
                bool down = x == xs[pane] + widths[pane] - 2 && y == 10;
#ifdef BUTTON5_PRESSED
                down = down || (m.bstate & BUTTON5_PRESSED);
#endif
                int old = pane == 0 ? item : pane == 1 ? draft[item].fg : draft[item].bg;
                if (up || down) at = old + (up ? -1 : 1);
                if (at < 0) at = 0;
                if (at >= count) at = count - 1;
                if (!pane) item = at;
                else if (pane == 1) draft[item].fg = (unsigned char)at;
                else draft[item].bg = (unsigned char)at;
                continue;
            }
        }
        if (t == KEY_CODE_YES && (c == KEY_LEFT || c == KEY_RIGHT)) { focus = (focus + (c == KEY_LEFT ? 6 : 1)) % 7; continue; }
        if (focus < 3 && t == KEY_CODE_YES) {
            int at = focus == 0 ? item : focus == 1 ? draft[item].fg : draft[item].bg;
            int count = focus ? THEME_COLORS : THEME_ITEMS;
            if (c == KEY_UP) --at;
            else if (c == KEY_DOWN) ++at;
            else if (c == KEY_HOME) at = 0;
            else if (c == KEY_END) at = count - 1;
            else if (c == KEY_PPAGE) at -= 7;
            else if (c == KEY_NPAGE) at += 7;
            else if (c != KEY_ENTER) continue;
            if (at < 0) at = 0;
            if (at >= count) at = count - 1;
            if (!focus) item = at;
            else if (focus == 1) draft[item].fg = (unsigned char)at;
            else draft[item].bg = (unsigned char)at;
            if (c != KEY_ENTER) continue;
        }
        bool enter = (t == OK && (c == '\n' || c == '\r')) || (t == KEY_CODE_YES && c == KEY_ENTER);
        if (enter || (t == OK && c == ' ' && focus >= 3)) {
            if (focus == 3) memcpy(draft, default_theme, sizeof draft);
            else if (focus == 5) return;
            else if (focus == 6) help = !help;
            else { memcpy(theme, draft, sizeof theme); theme_apply(); settings_save(); return; }
        }
    }
}
