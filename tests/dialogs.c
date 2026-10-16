/* Headless ncurses dialog regression tests.
 * Author: Guillermo Petruzzi
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#define GUI_TEST
#include "../src/gui.c"
#include <assert.h>

/* A sentinel makes a swallowed Escape fail an assertion rather than leaving
 * a modal waiting indefinitely. Escape must return before consuming it. */
static void queue_cancel(void) {
    assert(flushinp() == OK);
    assert(ungetch(KEY_RESIZE) == OK);
    assert(unget_wch(27) == OK);
}
static void assert_cancel_consumed_once(void) {
    wint_t c;
    assert(key(&c) == KEY_CODE_YES && c == KEY_RESIZE);
}
static void test_escape_policy(void) {
    const char *original = getenv("ESCDELAY");
    char *saved = original ? copy_path(original) : NULL;
    assert(unsetenv("ESCDELAY") == 0);
    configure_escape(); assert(get_escdelay() == 50);
    assert(setenv("ESCDELAY", "200", 1) == 0);
    assert(set_escdelay(200) == OK);
    configure_escape(); assert(get_escdelay() == 200);
    if (saved) { assert(setenv("ESCDELAY", saved, 1) == 0); free(saved); }
    else assert(unsetenv("ESCDELAY") == 0);
    assert(set_escdelay(50) == OK);

    assert(unget_wch('O') == OK); assert(unget_wch(27) == OK);
    wint_t c; assert(dialog_key(&c) == ALT_INPUT && c == 'o');
    assert(ungetch(KEY_DOWN) == OK);
    assert(dialog_key(&c) == KEY_CODE_YES && c == KEY_DOWN);
}
static void test_cancel_dialogs(void) {
    char *path = NULL;
    queue_cancel(); assert(!path_prompt("Open", &path)); assert_cancel_consumed_once();
    queue_cancel(); assert(!path_prompt("Save As", &path)); assert_cancel_consumed_once();
    assert(path == NULL);
    queue_cancel(); search_dialog(false); assert_cancel_consumed_once();
    queue_cancel(); search_dialog(true); assert_cancel_consumed_once();
    queue_cancel(); about(); assert_cancel_consumed_once();
    queue_cancel(); assert(confirm("Unsaved Changes", "Save changes?") == 2); assert_cancel_consumed_once();
    queue_cancel(); assert(!overwrite_prompt("existing.txt")); assert_cancel_consumed_once();
    unsigned tabs = tab_stops;
    queue_cancel(); settings_dialog(); assert_cancel_consumed_once(); assert(tab_stops == tabs);
    ThemeColor before[THEME_ITEMS]; memcpy(before, theme, sizeof before);
    queue_cancel(); colors_dialog(); assert_cancel_consumed_once(); assert(!memcmp(before, theme, sizeof before));
    queue_cancel(); assert(menu(0) == NONE); assert_cancel_consumed_once(); menu_keys = false;
    split_window(); assert(split_enabled);
    int original_row = split_row;
    queue_cancel(); size_window(); assert_cancel_consumed_once(); assert(split_row == original_row);
    close_window(); assert(!split_enabled);
}
static void write_file(const char *path, const char *text) {
    FILE *file = fopen(path, "w"); assert(file);
    assert(fputs(text, file) >= 0); assert(fclose(file) == 0);
}
static void test_open_selected_file(void) {
    char dir[] = "/tmp/gui-dialog-test-XXXXXX"; assert(mkdtemp(dir));
    char *previous = realpath(".", NULL); assert(previous);
    char *file = path_join(dir, "sample.txt"), *folder = path_join(dir, "folder");
    assert(mkdir(folder, 0700) == 0);
    char *nested = path_join(folder, "nested.txt");
    write_file(file, "selected file"); write_file(nested, "nested file");
    assert(chdir(dir) == 0);

    /* Tab to Files, then one Enter must open the file, not just fill its name. */
    queue_cancel();
    assert(unget_wch('\n') == OK); assert(unget_wch('\t') == OK);
    assert(action(OPEN));
    assert(e.path && !strcmp(e.path, file) && !wcscmp(e.s, L"selected file"));
    assert(flushinp() == OK);

    /* Enter on a directory still navigates; Enter on its file opens directly. */
    queue_cancel();
    assert(unget_wch('\n') == OK); assert(ungetch(KEY_LEFT) == OK);
    assert(unget_wch('\n') == OK); assert(ungetch(KEY_DOWN) == OK);
    assert(unget_wch('\t') == OK); assert(unget_wch('\t') == OK);
    char *opened = NULL;
    assert(path_prompt("Open", &opened)); assert(opened && !strcmp(opened, nested));
    free(opened); assert(flushinp() == OK);

    /* Direct opening uses the same read-only options as the OK button. */
    open_options.read_only = true;
    queue_cancel(); assert(unget_wch('\n') == OK); assert(unget_wch('\t') == OK);
    assert(action(OPEN)); assert(e.read_only && !wcscmp(e.s, L"selected file"));
    assert(flushinp() == OK);
    assert(chdir(previous) == 0);
    assert(unlink(file) == 0 && unlink(nested) == 0 && rmdir(folder) == 0 && rmdir(dir) == 0);
    free(previous); free(file); free(folder); free(nested);
}
static void test_untitled_open(void) {
    char dir[] = "/tmp/gui-untitled-test-XXXXXX"; assert(mkdtemp(dir));
    char *file = path_join(dir, "file.txt"); write_file(file, "hello");
    OpenOptions options = {false, false, 70};
    int blank = document_new(); assert(blank >= 0);
    workspace_open(file, &options);
    assert(active_document == blank && e.path && !strcmp(e.path, file));
    int loaded = active_document;
    blank = document_new(); assert(blank >= 0);
    split_window();
    workspace_open(file, &options);
    assert(active_document == loaded && !documents[blank].s);
    assert(panes[0].doc == loaded && panes[1].doc == loaded);
    close_window();

    /* A placeholder in another slot is removed after a successful open. */
    blank = document_new(); assert(blank >= 0);
    document_select(loaded);
    options.read_only = true; workspace_open(file, &options);
    assert(e.read_only && !documents[blank].s);

    /* Preserve unnamed edits and preserve placeholders when opening fails. */
    int edited = document_new(); assert(edited >= 0);
    reset_document(copy_text(L"draft", 5), 5, NULL, false, false);
    options.read_only = false; workspace_open(file, &options);
    assert(documents[edited].s && !wcscmp(documents[edited].s, L"draft"));
    blank = document_new(); assert(blank >= 0);
    workspace_open(dir, &options);
    assert(active_document == blank && !e.path && !e.n);
    document_select(loaded); document_free(blank); document_free(edited);
    assert(unlink(file) == 0 && rmdir(dir) == 0); free(file);
}
int main(void) {
    assert(setlocale(LC_ALL, "C.UTF-8"));
    /* Explicit terminfo and temporary streams avoid a physical display or TTY.
     * Queued keys exercise the actual modal event loops without user input. */
    FILE *input = tmpfile(), *output = tmpfile(); assert(input && output);
    SCREEN *screen = newterm("xterm", output, input); assert(screen);
    assert(resizeterm(26, 100) == OK);
    noecho(); keypad(stdscr, TRUE); configure_escape(); start_color();
    for (int pair = EDIT; pair <= PREVIEW; ++pair)
        assert(init_pair((short)pair, COLOR_BLACK, COLOR_WHITE) == OK);
    e.s = copy_text(L"", 0); e.saved = copy_text(L"", 0); e.preferred = -1;
    memcpy(theme, default_theme, sizeof theme);
    test_escape_policy(); test_cancel_dialogs(); test_open_selected_file(); test_untitled_open();
    endwin(); delscreen(screen); fclose(input); fclose(output);
    for (int i = 0; i < MAX_DOCS; ++i) document_free(i);
    puts("Dialog tests: OK"); return 0;
}
