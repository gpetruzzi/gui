/* Core behavior tests; no terminal initialization. SPDX-License-Identifier: MIT */
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
int main(void) {
    assert(setlocale(LC_ALL, "C.UTF-8"));
    e.s = copy_text(L"", 0); e.saved = copy_text(L"", 0); e.preferred = -1;
    test_selection_and_undo(); test_search_and_replace();
    test_lines_and_history_limit(); test_utf8_and_disk();
    test_file_picker_lists();
    test_read_only_and_binary_view();
    test_edit_menu_commands();
    free(e.s); free(e.saved); free(e.path); free(e.clip);
    clear_history(e.undo, &e.un); clear_history(e.redo, &e.rn);
    puts("Core tests: OK"); return 0;
}

