/* Core behavior tests; no terminal initialization. SPDX-License-Identifier: GPL-3.0-or-later
 * Author: Guillermo Petruzzi
 * Copyright (c) 2026 Guillermo Petruzzi and gui contributors
 */
#define GUI_TEST
#include "../src/gui.c"
#include <assert.h>

static void document(const wchar_t *s) {
    reset_document(copy_text(s, wcslen(s)), wcslen(s), NULL, false, false);
}
static void test_selection_and_undo(void) {
    document(L"hola\nmundo");
    e.anchor = 1; e.pos = 4;
    copy_selection(false);
    assert(e.clip_n == 3 && !wcscmp(e.clip, L"ola"));
    insert(L"i", 1);
    assert(!wcscmp(e.s, L"hi\nmundo") && dirty());
    undo_redo(false);
    assert(!wcscmp(e.s, L"hola\nmundo") && !dirty());
    undo_redo(true);
    assert(!wcscmp(e.s, L"hi\nmundo"));
    e.pos = e.anchor = e.n;
    insert(e.clip, e.clip_n);
    assert(!wcscmp(e.s, L"hi\nmundoola"));
}
static void test_search_and_replace(void) {
    document(L"sol solar SOL\nsol");
    wcscpy(e.needle, L"sol"); wcscpy(e.replacement, L"luna");
    e.match_case = false; e.whole_word = true;
    assert(find_next(0) && e.anchor == 0 && e.pos == 3);
    assert(find_next(e.pos) && e.anchor == 10);
    assert(replace_all());
    assert(!wcscmp(e.s, L"luna solar luna\nluna"));
    undo_redo(false);
    assert(!wcscmp(e.s, L"sol solar SOL\nsol"));
    e.match_case = true;
    assert(replace_all());
    assert(!wcscmp(e.s, L"luna solar SOL\nluna"));
    wcscpy(e.needle, L"luna"); wcscpy(e.replacement, L"");
    assert(replace_all());
    assert(!wcscmp(e.s, L" solar SOL\n"));
    e.needle[0] = 0;
    assert(!replace_all());
}
static void test_lines_and_history_limit(void) {
    document(L"ab\n12345\nz");
    assert(mouse_position(EDIT_TOP, EDIT_LEFT) == 0);
    assert(mouse_position(EDIT_TOP, EDIT_LEFT + 1) == 1);
    assert(mouse_position(EDIT_TOP + 1, EDIT_LEFT + 2) == 5);
    assert(menu_from_x(1) == 0 && menu_from_x(13) == 2 && menu_from_x(36) == 5);
    e.pos = e.anchor = 2;
    vertical(1, true);
    assert(e.anchor == 2 && e.pos == 5);
    vertical(1, true);
    assert(e.pos == e.n);
    vertical(-1, false);
    assert(e.pos == 5 && e.anchor == 5);
    document(L"");
    for (int i = 0; i < HISTORY + 10; ++i) insert(L"x", 1);
    assert(e.un == HISTORY);
    for (int i = 0; i < HISTORY; ++i) undo_redo(false);
    assert(e.n == 10);
    insert(L"y", 1);
    assert(e.rn == 0);
}
static void test_utf8_and_disk(void) {
    char dir[] = "/tmp/gui-test-XXXXXX";
    assert(mkdtemp(dir));
    char path[256]; snprintf(path, sizeof path, "%s/texto.txt", dir);
    document(L"áéñ\nsegunda\n");
    e.path = copy_path(path); e.crlf = true; e.bom = true;
    assert(save());
    FILE *f = fopen(path, "rb"); assert(f);
    unsigned char bytes[256]; size_t n = fread(bytes, 1, sizeof bytes, f); fclose(f);
    const char expected[] = "\xef\xbb\xbf\xc3\xa1\xc3\xa9\xc3\xb1\r\nsegunda\r\n";
    assert(n == sizeof expected - 1 && !memcmp(bytes, expected, n));
    assert(load(path));
    assert(e.crlf && e.bom && !dirty() && !wcscmp(e.s, L"áéñ\nsegunda\n"));
    f = fopen(path, "wb"); assert(f);
    assert(fwrite("bad\0data", 1, 8, f) == 8); fclose(f);
    assert(!load(path));
    assert(!wcscmp(e.s, L"áéñ\nsegunda\n"));
    assert(unlink(path) == 0); assert(rmdir(dir) == 0);
}
static void test_file_picker_lists(void) {
    char dir[] = "/tmp/gui-browser-test-XXXXXX";
    assert(mkdtemp(dir));
    char *before = realpath(".", NULL); assert(before);
    char *file = path_join(dir, "sample.txt"), *subdir = path_join(dir, "folder");
    assert(mkdir(subdir, 0700) == 0);
    FILE *f = fopen(file, "wb"); assert(f); fclose(f);
    FileList files = {0}, dirs = {0};
    assert(directory_scan(dir, &files, &dirs));
    assert(files.n == 1 && !strcmp(files.names[0], "sample.txt"));
    assert(dirs.n == 2 && !strcmp(dirs.names[0], "..") && !strcmp(dirs.names[1], "folder"));
    char *missing = path_join(dir, "missing");
    assert(!directory_scan(missing, &files, &dirs));
    assert(files.n == 1 && dirs.n == 2);
    char *after = realpath(".", NULL); assert(after && !strcmp(before, after));
    wchar_t name[FIELD] = {0}; size_t pos = 0;
    assert(file_name_set(name, &pos, "áé.txt") && !wcscmp(name, L"áé.txt") && pos == 6);
    char *absolute = path_join(dir, "/tmp/gui-absolute"); assert(!strcmp(absolute, "/tmp/gui-absolute"));
    list_free(&files); list_free(&dirs);
    assert(unlink(file) == 0); assert(rmdir(subdir) == 0); assert(rmdir(dir) == 0);
    free(before); free(after); free(file); free(subdir); free(missing); free(absolute);
}
static void test_read_only_and_binary_view(void) {
    unsigned char bytes[] = {0, 255, 'A', '\n'};
    size_t length;
    wchar_t *view = hex_document(bytes, sizeof bytes, 70, &length);
    assert(!wcscmp(view, L"00000000: 00 FF 41 0A |..A.|\n") && length == wcslen(view));
    free(view);
    view = hex_document(bytes, 2, 1, &length);
    assert(!wcscmp(view, L"00000000: 00 |.|\n00000001: FF |.|\n")); free(view);
    char path[] = "/tmp/gui-binary-test-XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0);
    assert(write(fd, bytes, sizeof bytes) == (ssize_t)sizeof bytes); assert(close(fd) == 0);
    document(L"preserved");
    assert(!load(path) && !wcscmp(e.s, L"preserved"));
    const OpenOptions binary = {true, true, 70};
    assert(load_mode(path, &binary) && e.read_only && e.binary_view && !dirty());
    size_t old = e.n; insert(L"x", 1); assert(e.n == old && !dirty());
    e.anchor = 0; e.pos = 2; copy_selection(true);
    assert(e.n == old && !dirty() && !wcscmp(e.clip, L"00"));
    assert(!save());
    FILE *f = fopen(path, "rb"); assert(f);
    unsigned char actual[4]; assert(fread(actual, 1, sizeof actual, f) == sizeof actual); fclose(f);
    assert(!memcmp(actual, bytes, sizeof bytes));
    f = fopen(path, "wb"); assert(f); assert(fwrite("text", 1, 4, f) == 4); fclose(f);
    const OpenOptions text = {true, false, 70};
    assert(load_mode(path, &text) && e.read_only && !e.binary_view);
    insert(L"z", 1); assert(!wcscmp(e.s, L"text"));
    document(L"editable"); assert(!e.read_only && !e.binary_view);
    assert(unlink(path) == 0);
}
static void test_edit_menu_commands(void) {
    document(L"abc");
    free(e.clip); e.clip = NULL; e.clip_n = 0;
    assert(!action_enabled(CUT) && !action_enabled(COPY) && !action_enabled(PASTE) && !action_enabled(CLEAR));
    e.anchor = 0; e.pos = 1; copy_selection(false);
    e.anchor = 1; e.pos = 3;
    assert(action_enabled(CUT) && action_enabled(COPY) && action_enabled(PASTE) && action_enabled(CLEAR));
    action(CLEAR);
    assert(!wcscmp(e.s, L"a") && !wcscmp(e.clip, L"a"));
    undo_redo(false); assert(!wcscmp(e.s, L"abc"));
    e.read_only = true;
    assert(action_enabled(COPY) && !action_enabled(CUT) && !action_enabled(PASTE) && !action_enabled(CLEAR));
    action(CLEAR); assert(!wcscmp(e.s, L"abc"));
    document(L"");
}
static void test_mouse_drag_copy_paste(void) {
    document(L"abcdef\nxyz");
    mouse_select(1, BUTTON1_PRESSED);
    assert(e.dragging && e.anchor == 1 && e.pos == 1);
    /* Some terminals report held-button motion as another PRESSED event. */
    mouse_select(3, BUTTON1_PRESSED);
    assert(e.anchor == 1 && e.pos == 3);
    mouse_select(4, REPORT_MOUSE_POSITION);
    assert(e.anchor == 1 && e.pos == 4);
    mouse_select(5, BUTTON1_RELEASED);
    assert(!e.dragging && e.anchor == 1 && e.pos == 5);
    copy_selection(false); assert(e.clip_n == 4 && !wcscmp(e.clip, L"bcde"));
    move_to(e.n, false, false); action(PASTE);
    assert(!wcscmp(e.s, L"abcdef\nxyzbcde"));
    undo_redo(false); assert(!wcscmp(e.s, L"abcdef\nxyz"));
    mouse_select(4, BUTTON1_PRESSED); mouse_select(1, BUTTON1_PRESSED);
    mouse_select(2, BUTTON1_RELEASED);
    copy_selection(false); assert(!wcscmp(e.clip, L"cd"));
    e.anchor = 1; e.pos = 2;
    mouse_select(4, BUTTON1_CLICKED | BUTTON_SHIFT);
    assert(e.anchor == 1 && e.pos == 4 && !e.dragging);
    mouse_select(6, REPORT_MOUSE_POSITION); assert(e.pos == 4);
    e.dragging = true; assert(handle(CTRL('c'), OK));
    assert(!e.dragging && !wcscmp(e.clip, L"bcd"));
    mouse_select(0, BUTTON1_CLICKED);
    assert(e.pos == 0 && e.anchor == 0 && !e.dragging);
    document(L"");
}
static void test_cursor_repaint_state(void) {
    document(L"abcdef\nxyz");
    ViewState before = view_snapshot(20, 78);
    move_to(3, false, false);
    ViewState after = view_snapshot(20, 78);
    assert(!view_changed(&before, &after));
    move_to(5, true, false); after = view_snapshot(20, 78);
    assert(view_changed(&before, &after));
    before = after; copy_selection(false); after = view_snapshot(20, 78);
    assert(!view_changed(&before, &after));
    move_to(2, false, false); before = view_snapshot(20, 78);
    e.left = 2; after = view_snapshot(20, 78); assert(view_changed(&before, &after));
    e.left = 0; after = view_snapshot(19, 78); assert(view_changed(&before, &after));
    insert(L"z", 1); after = view_snapshot(20, 78); assert(view_changed(&before, &after));
    before = after; undo_redo(false); after = view_snapshot(20, 78);
    assert(view_changed(&before, &after));
    before = after; e.read_only = true; after = view_snapshot(20, 78);
    assert(view_changed(&before, &after));
    document(L"");
}
static void test_documents_and_split(void) {
    document(L"first");
    e.anchor = 0; e.pos = 5; copy_selection(false);
    int first = active_document, second = document_new();
    assert(second >= 0 && second != first && !wcscmp(e.clip, L"first"));
    action(PASTE); assert(!wcscmp(e.s, L"first"));
    insert(L" second", 7);
    document_select(first); assert(!wcscmp(e.s, L"first"));
    document_select(second); assert(!wcscmp(e.s, L"first second"));
    undo_redo(false); assert(!wcscmp(e.s, L"first"));
    document_select(first); assert(!dirty());
    int old_lines = LINES, old_cols = COLS;
    LINES = 24; COLS = 80;
    move_to(1, false, false); split_window();
    assert(split_enabled && panes[0].doc == first && panes[1].doc == first);
    pane_select(1); move_to(4, false, false);
    pane_select(0); assert(e.pos == 1);
    insert(L"X", 1);
    assert(!wcscmp(e.s, L"fXirst") && panes[1].pos == 5);
    pane_select(1); assert(e.pos == 5);
    document_select(second);
    assert(panes[0].doc == first && panes[1].doc == second);
    move_to(2, false, false); pane_select(0); assert(e.pos == 2);
    pane_select(1); assert(e.pos == 2 && active_document == second);
    close_window(); assert(!split_enabled && active_document == first);
    document_select(second); document(L""); close_window();
    assert(active_document == first && documents[second].s == NULL);
    document(L"");
    assert(column(0) == 0);
    tab_stops = 8; document(L"a\tb"); assert(column(2) == 8);
    tab_stops = 3; assert(column(2) == 3 && at_column(0, 2) == 1);
    tab_stops = 8;
    for (int i = 1; i < MAX_DOCS; ++i) assert(document_new() >= 0);
    assert(document_new() == -1);
    document_select(first);
    for (int i = 0; i < MAX_DOCS; ++i) if (i != first) document_free(i);
    LINES = old_lines; COLS = old_cols;
    document(L"");
}
static void test_workspace_open(void) {
    document(L"important"); insert(L"!", 1);
    int original = active_document;
    char path[] = "/tmp/gui-workspace-XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0);
    assert(write(fd, "loaded", 6) == 6 && close(fd) == 0);
    const OpenOptions options = {false, false, 70};
    workspace_open(path, &options);
    int opened = active_document;
    assert(opened != original && !wcscmp(e.s, L"loaded"));
    assert(!wcscmp(documents[original].s, L"!important"));
    workspace_open(path, &options); assert(active_document == opened);
    FILE *f = fopen(path, "wb"); assert(f);
    assert(fwrite("bad\0data", 1, 8, f) == 8); fclose(f);
    /* A different path reaches the load-failure branch rather than deduplication. */
    char *other = alloc(strlen(path) + 5); sprintf(other, "%s-bad", path);
    assert(rename(path, other) == 0);
    workspace_open(other, &options);
    assert(active_document == opened && !wcscmp(e.s, L"loaded") && e.message[0]);
    int count = 0;
    for (int i = 0; i < MAX_DOCS; ++i) if (documents[i].s) ++count;
    assert(count == 2);
    close_window(); assert(active_document == original && dirty());
    assert(unlink(other) == 0); free(other); document(L"");
}
static void test_preferences_roundtrip(void) {
    char base[] = "/tmp/gui-preferences-XXXXXX"; assert(mkdtemp(base));
    const char *prior = getenv("XDG_CONFIG_HOME"); char *saved_env = prior ? copy_path(prior) : NULL;
    assert(setenv("XDG_CONFIG_HOME", base, 1) == 0);
    settings_load(); assert(tab_stops == 8 && !memcmp(theme, default_theme, sizeof theme));
    tab_stops = 6; theme[0] = (ThemeColor){14, 4}; settings_save();
    tab_stops = 2; theme[0] = (ThemeColor){0, 0}; settings_load();
    assert(tab_stops == 6 && theme[0].fg == 14 && theme[0].bg == 4);
    char *dir = path_join(base, "gui"), *path = path_join(dir, "settings");
    FILE *f = fopen(path, "w"); assert(f); fputs("gui 1\ntabs 0\n99 99\n", f); fclose(f);
    settings_load(); assert(tab_stops == 8 && !memcmp(theme, default_theme, sizeof theme));
    assert(unlink(path) == 0 && rmdir(dir) == 0 && rmdir(base) == 0);
    free(dir); free(path);
    if (saved_env) { assert(setenv("XDG_CONFIG_HOME", saved_env, 1) == 0); free(saved_env); }
    else assert(unsetenv("XDG_CONFIG_HOME") == 0);
}
static void test_dialog_palette(void) {
    int previous_colors = COLORS;
    COLORS = 256;
    assert(color_number(0) == 16 && color_number(7) == 248 && color_number(15) == 231);
    assert((control_attrs(BUTTON) & (A_BOLD | A_REVERSE)) == 0);
    assert((control_attrs(TITLE) & (A_BOLD | A_REVERSE)) == 0);
    COLORS = 16; assert(color_number(0) == 0 && color_number(15) == 15);
    COLORS = previous_colors;
}
int main(void) {
    assert(setlocale(LC_ALL, "C.UTF-8"));
    e.s = copy_text(L"", 0); e.saved = copy_text(L"", 0); e.preferred = -1;
    test_selection_and_undo(); test_search_and_replace();
    test_lines_and_history_limit(); test_utf8_and_disk();
    test_file_picker_lists();
    test_read_only_and_binary_view();
    test_edit_menu_commands();
    test_mouse_drag_copy_paste();
    test_cursor_repaint_state();
    test_documents_and_split();
    test_workspace_open();
    test_preferences_roundtrip();
    test_dialog_palette();
    for (int i = 0; i < MAX_DOCS; ++i) document_free(i);
    puts("Core tests: OK"); return 0;
}
