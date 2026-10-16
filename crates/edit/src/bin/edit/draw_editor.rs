// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

use edit::framebuffer::IndexedColor;
use edit::helpers::*;
use edit::icu;
use edit::input::{kbmod, vk};
use edit::tui::*;

use crate::localization::*;
use crate::state::*;

pub fn draw_editor(ctx: &mut Context, state: &mut State) {
    let size = ctx.size();

    if let Some(doc) = state.documents.active() {
        ctx.textarea("textarea", doc.buffer.clone());
        ctx.inherit_focus();
    } else {
        ctx.block_begin("empty");
        ctx.block_end();
    }

    ctx.attr_intrinsic_size(Size { width: 0, height: (size.height - 2).max(0) });

    if !matches!(state.wants_search.kind, StateSearchKind::Hidden | StateSearchKind::Disabled) {
        draw_search(ctx, state);
    }
}

fn draw_search(ctx: &mut Context, state: &mut State) {
    if let Err(err) = icu::init() {
        error_log_add(ctx, state, err.into());
        state.wants_search.kind = StateSearchKind::Disabled;
        return;
    }

    let Some(doc) = state.documents.active() else {
        state.wants_search.kind = StateSearchKind::Hidden;
        return;
    };

    let replacing = state.wants_search.kind == StateSearchKind::Replace;
    let mut action = None;
    let mut cancelled = false;
    let mut focus = StateSearchKind::Hidden;

    if state.wants_search.focus {
        state.wants_search.focus = false;
        focus = StateSearchKind::Search;

        // If the selection is empty, focus the search input field.
        // Otherwise, focus the replace input field, if it exists.
        if let Some(selection) = doc.buffer.borrow_mut().extract_user_selection(false) {
            state.search_needle = string_from_utf8_lossy_owned(selection);
            focus = state.wants_search.kind;
        }
    }

    let field_width = (ctx.size().width - 26).clamp(1, 26);
    ctx.classic_modal_begin(
        "search",
        if replacing { loc(LocId::EditReplace) } else { loc(LocId::EditFind) },
    );
    ctx.block_begin("content");
    ctx.attr_padding(Rect::three(1, 2, 1));
    {
        if ctx.contains_focus() && ctx.consume_shortcut(vk::F3) {
            action = Some(SearchAction::Search);
        }
        ctx.table_begin("needle");
        ctx.table_set_cell_gap(Size { width: 1, height: 0 });
        {
            {
                ctx.table_next_row();
                ctx.label("label", loc(LocId::SearchNeedleLabel));

                if ctx.classic_editline("needle", &mut state.search_needle) {
                    state.search_success = true;
                }
                if !state.search_success {
                    ctx.attr_background_rgba(ctx.indexed(IndexedColor::Red));
                    ctx.attr_foreground_rgba(ctx.indexed(IndexedColor::BrightWhite));
                }
                ctx.attr_intrinsic_size(Size { width: field_width, height: 1 });
                if focus == StateSearchKind::Search {
                    ctx.steal_focus();
                }
                if ctx.is_focused() && ctx.consume_shortcut(vk::RETURN) {
                    action = Some(if replacing {
                        SearchAction::Replace
                    } else {
                        SearchAction::Search
                    });
                }
            }

            if state.wants_search.kind == StateSearchKind::Replace {
                ctx.table_next_row();
                ctx.label("label", loc(LocId::SearchReplacementLabel));

                ctx.classic_editline("replacement", &mut state.search_replacement);
                ctx.attr_intrinsic_size(Size { width: field_width, height: 1 });
                if focus == StateSearchKind::Replace {
                    ctx.steal_focus();
                }
                if ctx.is_focused() {
                    if ctx.consume_shortcut(vk::RETURN) {
                        action = Some(SearchAction::Replace);
                    } else if ctx.consume_shortcut(kbmod::CTRL_ALT | vk::RETURN) {
                        action = Some(SearchAction::ReplaceAll);
                    }
                }
            }
        }
        ctx.table_end();

        ctx.table_begin("options");
        ctx.attr_padding(Rect::three(1, 0, 1));
        {
            ctx.table_next_row();
            ctx.classic_checkbox(
                "whole-word",
                "Solo palabras completas",
                &mut state.search_options.whole_word,
            );
            ctx.table_next_row();
            ctx.classic_checkbox(
                "match-case",
                "Distinguir mayúsculas/minúsculas",
                &mut state.search_options.match_case,
            );
        }
        ctx.table_end();

        if !state.search_success {
            ctx.label("not-found", "No se encontraron coincidencias.");
        }

        if replacing {
            ctx.table_begin("replace-all-row");
            ctx.attr_padding(Rect::three(0, 0, 1));
            ctx.table_next_row();
            if ctx.classic_button("replace-all", loc(LocId::SearchReplaceAll)) {
                action = Some(SearchAction::ReplaceAll);
            }
            ctx.table_end();
        }

        ctx.table_begin("choices");
        ctx.attr_position(Position::Center);
        ctx.table_set_cell_gap(Size { width: 3, height: 0 });
        ctx.table_next_row();
        {
            if ctx.classic_button("ok", "  OK  ") {
                action = Some(if replacing {
                    SearchAction::Replace
                } else {
                    SearchAction::Search
                });
            }
            if ctx.classic_button("cancel", loc(LocId::Cancel)) {
                cancelled = true;
            }
            if ctx.classic_button("help", loc(LocId::Help)) {
                state.search_help = !state.search_help;
            }
        }
        ctx.table_end();

        if state.search_help {
            ctx.label("help-tab", "Tab / Shift+Tab: cambiar de campo.");
            ctx.attr_padding(Rect::three(1, 0, 0));
            ctx.label("help-keys", "Enter: aceptar. Esc: cancelar. F3: siguiente.");
        }
    }
    ctx.block_end();
    cancelled |= ctx.modal_end();

    if cancelled {
        state.wants_search.kind = StateSearchKind::Hidden;
        state.search_help = false;
        ctx.needs_rerender();
    } else if let Some(action) = action {
        search_execute(ctx, state, action);
        if state.search_success {
            state.wants_search.kind = StateSearchKind::Hidden;
            state.search_help = false;
        }
    }
}

pub enum SearchAction {
    Search,
    Replace,
    ReplaceAll,
}

pub fn search_execute(ctx: &mut Context, state: &mut State, action: SearchAction) {
    let Some(doc) = state.documents.active_mut() else {
        return;
    };

    state.search_success = match action {
        SearchAction::Search => {
            doc.buffer.borrow_mut().find_and_select(&state.search_needle, state.search_options)
        }
        SearchAction::Replace => doc.buffer.borrow_mut().find_and_replace(
            &state.search_needle,
            state.search_options,
            state.search_replacement.as_bytes(),
        ),
        SearchAction::ReplaceAll => doc.buffer.borrow_mut().find_and_replace_all(
            &state.search_needle,
            state.search_options,
            state.search_replacement.as_bytes(),
        ),
    }
    .is_ok();

    ctx.needs_rerender();
}

pub fn draw_handle_save(ctx: &mut Context, state: &mut State) {
    if let Some(doc) = state.documents.active_mut() {
        if doc.path.is_some() {
            if let Err(err) = doc.save(None) {
                error_log_add(ctx, state, err);
            }
        } else {
            // No path? Show the file picker.
            state.wants_file_picker = StateFilePicker::SaveAs;
            state.wants_save = false;
            ctx.needs_rerender();
        }
    }

    state.wants_save = false;
}

pub fn draw_handle_wants_close(ctx: &mut Context, state: &mut State) {
    let Some(doc) = state.documents.active() else {
        state.wants_close = false;
        return;
    };

    if !doc.buffer.borrow().is_dirty() {
        state.documents.remove_active();
        state.wants_close = false;
        ctx.needs_rerender();
        return;
    }

    enum Action {
        None,
        Save,
        Discard,
        Cancel,
    }
    let mut action = Action::None;

    ctx.modal_begin("unsaved-changes", loc(LocId::UnsavedChangesDialogTitle));
    ctx.attr_background_rgba(ctx.indexed(IndexedColor::Red));
    ctx.attr_foreground_rgba(ctx.indexed(IndexedColor::BrightWhite));
    {
        let contains_focus = ctx.contains_focus();

        ctx.label("description", loc(LocId::UnsavedChangesDialogDescription));
        ctx.attr_padding(Rect::three(1, 2, 1));

        ctx.table_begin("choices");
        ctx.inherit_focus();
        ctx.attr_padding(Rect::three(0, 2, 1));
        ctx.attr_position(Position::Center);
        ctx.table_set_cell_gap(Size { width: 2, height: 0 });
        {
            ctx.table_next_row();
            ctx.inherit_focus();

            if ctx.button(
                "yes",
                loc(LocId::UnsavedChangesDialogYes),
                ButtonStyle::default().accelerator('S'),
            ) {
                action = Action::Save;
            }
            ctx.inherit_focus();
            if ctx.button(
                "no",
                loc(LocId::UnsavedChangesDialogNo),
                ButtonStyle::default().accelerator('N'),
            ) {
                action = Action::Discard;
            }
            if ctx.button("cancel", loc(LocId::Cancel), ButtonStyle::default()) {
                action = Action::Cancel;
            }

            // Handle accelerator shortcuts
            if contains_focus {
                if ctx.consume_shortcut(vk::S) {
                    action = Action::Save;
                } else if ctx.consume_shortcut(vk::N) {
                    action = Action::Discard;
                }
            }
        }
        ctx.table_end();
    }
    if ctx.modal_end() {
        action = Action::Cancel;
    }

    match action {
        Action::None => return,
        Action::Save => {
            state.wants_save = true;
        }
        Action::Discard => {
            state.documents.remove_active();
            state.wants_close = false;
        }
        Action::Cancel => {
            state.wants_exit = false;
            state.wants_close = false;
        }
    }

    ctx.needs_rerender();
}

pub fn draw_goto_menu(ctx: &mut Context, state: &mut State) {
    let mut done = false;

    if let Some(doc) = state.documents.active_mut() {
        ctx.modal_begin("goto", loc(LocId::FileGoto));
        {
            if ctx.editline("goto-line", &mut state.goto_target) {
                state.goto_invalid = false;
            }
            if state.goto_invalid {
                ctx.attr_background_rgba(ctx.indexed(IndexedColor::Red));
                ctx.attr_foreground_rgba(ctx.indexed(IndexedColor::BrightWhite));
            }

            ctx.attr_intrinsic_size(Size { width: 24, height: 1 });
            ctx.steal_focus();

            if ctx.consume_shortcut(vk::RETURN) {
                if let Some(goto) = validate_goto_point(&state.goto_target) {
                    doc.cursor_move_to_goto(goto);
                    doc.buffer.borrow_mut().make_cursor_visible();
                    done = true;
                } else {
                    state.goto_invalid = true;
                }
                ctx.needs_rerender();
            }
        }
        done |= ctx.modal_end();
    } else {
        done = true;
    }

    if done {
        state.wants_goto = false;
        state.goto_target.clear();
        state.goto_invalid = false;
        ctx.needs_rerender();
    }
}

fn validate_goto_point(line: &str) -> Option<Point> {
    let mut coords = [0; 2];
    let (y, x) = line.split_once(':').unwrap_or((line, "1"));
    // Using a loop here avoids 2 copies of the str->int code.
    // This makes the binary more compact.
    for (i, s) in [x, y].iter().enumerate() {
        coords[i] = s.parse::<CoordType>().ok()?;
    }
    // Counting backwards is only supported for lines.
    if coords[0] < 1 {
        return None;
    }
    Some(Point { x: coords[0], y: coords[1] })
}

