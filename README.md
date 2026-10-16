# gui — C edition

An independent Linux terminal editor written from scratch in C11 with ncursesw.
The `c-rewrite` branch contains new C code, not a translation of Microsoft Edit's Rust implementation. The older Rust variant remains in `main`.

**Development prototype. The latest interface changes have not been compiled or visually verified by the assistant.** Try this branch separately before replacing an installed editor.

## Classic appearance

- English interface: File, Edit, Search, View, Options and Help.
- Blue editing area with white text, a thin frame, and a centered file name along a thin white title rule.
- Light gray menu bar and bottom help bar with black text.
- The compact Edit menu contains Cut, Copy, Paste and Clear, with gray unavailable commands, a black selection bar and a contextual description in the bottom bar. Clear deletes only the selection and keeps the internal clipboard unchanged. Undo, Redo and Select All remain available through their keyboard shortcuts.
- No line numbers and no current-line highlight. Only explicitly selected text changes color.
- Gray dialogs with white title strips, dotted input fields, checkboxes and white buttons with thin black shadows. Labels highlight their Alt access letter in white; button access letters are underlined in black for contrast against the white button face.
- File and directory scrollbars use white arrows on black, a patterned track and a light proportional thumb. These styles are shared by all applicable dialogs.
- Open and Save As use a classic two-pane browser: File Name, current path, Existing Files and Directories, with OK, Cancel and Help buttons. Use Tab to move between controls, arrows to select an entry and Enter to enter a directory. Saving to another existing file asks before replacing it.
- Open has a taller Files pane and a shorter Directories pane, followed by Open Read-only, Open Binary and Line Width. Read-only blocks edits and saving. Open Binary opens a **read-only hexadecimal view**, not a binary editor; Line Width is the number of source bytes per row (1–256, default 70). It is disabled for ordinary text files.

The terminal font controls the exact appearance of characters and shadows. Buttons use terminal line characters for thin shadows and ASCII focus markers to avoid missing Unicode glyphs. Standard ANSI colors avoid relying on terminal palette reprogramming.

The editor status bar shows the current one-based Line and Col at the right; columns account for tabs and wide characters. About gui stays open until Enter, Esc, Alt+O or a click on OK; unrelated mouse events do not dismiss it.

Ordinary cursor movement does not erase or rebuild the screen. The editor tracks text revisions, selection, scrolling and terminal size; only changes to those require repainting the document. Status messages update the footer independently. Dialogs and menus are composed before refreshing the terminal, avoiding an intermediate display of the uncovered background.

## Keys

| Action | Keys |
| --- | --- |
| Find / Replace | Ctrl+B / Ctrl+R |
| Find Next | F3 |
| Select text | Shift+arrows |
| Select All | Ctrl+A |
| Copy | Ctrl+C or Ctrl+Insert |
| Cut | Ctrl+X |
| Paste | Ctrl+V or Shift+Insert |
| Save / Open / New | Ctrl+S / Ctrl+O / Ctrl+N |
| Save As | File menu → Save As... |
| Quit | Ctrl+Q |
| Undo / Redo | Ctrl+Z / Ctrl+Y |
| File menu | Alt+A or Alt+F |
| Edit menu | Alt+E |
| Search menu | Alt+B or Alt+S |
| View / Options menus | Alt+V / Alt+O |
| Help menu | Alt+Y or Alt+H |
| Menu bar / About gui | F10 / F1 |

The clipboard is internal in this prototype. To paste from another application, use your terminal's paste command (usually Ctrl+Shift+V). Bracketed terminal paste is supported in the document. Some terminals intercept Ctrl+Insert or Shift+Insert.

To copy with the mouse, hold the left button and drag, release it, press Ctrl+C or choose Edit → Copy, then use Ctrl+V inside gui. The status bar reports the number of characters copied to gui's internal clipboard. Copying without a selection reports "No text selected." Drag handling preserves its anchor across repeated held-button events and ends on release, including outside the document.

Use Tab and Shift+Tab to move between dialog controls, Space to toggle checkboxes, Enter to activate a control, and Esc to cancel. The mouse can place the cursor, select text, open menus and activate search dialog controls.

Dialog Alt shortcuts: file browsers use N for File Name, F for Files, D for Directories, O for OK, C for Cancel and H for Help. Open adds R for Read-only, B for Binary and W for Line Width. Find/Replace use F for Find What, R for Replace With, W for Whole Word, M for Match Case, O for OK, A for All, C for Cancel and H for Help. Save confirmations use S for Save, D for Discard and C for Cancel.

## Build separately on Debian or Ubuntu

```sh
apt update
apt install build-essential libncurses-dev git
git clone --branch c-rewrite --single-branch https://github.com/gpetruzzi/gui.git gui-c
cd gui-c
make
make test
./gui sample.txt
```

For an existing checkout:

```sh
cd ~/gui-c
git pull --ff-only
make && make test
./gui sample.txt
```

Fedora dependencies: `gcc make ncurses-devel git`. Rust is not required.
After validating the prototype, `make install` installs to `/usr/local/bin` by default.

## Validation

Core tests cover selection, undo/redo, search, replacement, mouse coordinate mapping, UTF-8/CRLF file I/O, file browser directory scanning, read-only protection and binary hex views. Updated tests have not been run by the assistant.

On Linux, also check all selection shortcuts, menu activation, dialog focus, terminal resize, canceled exits with unsaved changes, and failed saves. Compare the appearance against the classic editor references.

## Current scope

One open document and literal search without regular expressions. Text editing requires UTF-8 without NUL bytes; other files can be inspected using Open Binary, a read-only hex view. UTF-8 BOM and uniform LF/CRLF line endings are preserved. Mixed line endings are normalized to CRLF when CRLF is present. Saving over symbolic links is not supported.

The document and up to 64 undo states are stored as memory copies, so this prototype is intended for small and medium text files. Editing operates on Unicode code points; complex emoji and combining sequences are not yet treated as indivisible grapheme clusters.

MIT License. The About dialog identifies gui's own C edition and version.
