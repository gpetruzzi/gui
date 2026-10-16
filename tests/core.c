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
int main(void) {
    assert(setlocale(LC_ALL, "C.UTF-8"));
    e.s = copy_text(L"", 0); e.saved = copy_text(L"", 0); e.preferred = -1;
    test_selection_and_undo(); test_search_and_replace();
    test_lines_and_history_limit(); test_utf8_and_disk();
    test_file_picker_lists();
    free(e.s); free(e.saved); free(e.path); free(e.clip);
    clear_history(e.undo, &e.un); clear_history(e.redo, &e.rn);
    puts("Core tests: OK"); return 0;
}

