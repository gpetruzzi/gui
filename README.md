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
- File and directory scrollbars use white arrows on black, white stipple on a black track and a solid white proportional thumb. These styles are shared by all applicable dialogs and editor panels.
- Open and Save As use a classic two-pane browser: File Name, current path, Existing Files and Directories, with OK, Cancel and Help buttons. Use Tab to move between controls, arrows to select an entry and Enter to enter a directory. Saving to another existing file asks before replacing it.
- Open has a taller Files pane and a shorter Directories pane, followed by Open Read-only, Open Binary and Line Width. Read-only blocks edits and saving. Open Binary opens a **read-only hexadecimal view**, not a binary editor; Line Width is the number of source bytes per row (1–256, default 70). It is disabled for ordinary text files.

The terminal font controls the exact appearance of characters and shadows. All dialogs share a full-width, one-row white title strip, white buttons with black text, inward ASCII arrow focus markers (`> OK <`) and a short half-cell black shadow offset down and right. The renderer uses a neutral curses background so blank controls keep their own colors. On 256-color terminals, fixed palette slots provide black (#000000), white (#ffffff) and a darker gray (#a8a8a8). Button text uses no bold or reverse video. A UTF-8 font containing half-block glyphs is required for those details. No terminal palette entries are reprogrammed.

gui respects the connection's TERM and never substitutes a different terminfo profile. Borders, arrows and scrollbar stipple are written as Unicode characters through ncursesw, avoiding VT100 alternate-character-set sequences that can appear as q/x letters in UTF-8 terminal sessions. Real eight-color terminals cannot display separate light gray and bright white backgrounds. Configure KiTTY's actual terminal capabilities before advertising a 256-color profile.

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
| Split Window / Size Window | Ctrl+F6 / Ctrl+F8 |
| Close Window | Ctrl+F4 |
| Next panel (or next document with a single panel) | F6 |
| Switch document | Alt+1 through Alt+9 |

Alt combinations highlight menu access letters in white. Esc alone toggles menu access mode; press F, E, S, V, O or H to open a menu. Alt alone can only be detected if the terminal sends an escape event for it. Many terminal emulators send nothing until the next key; F10 opens the menu bar reliably. Alt+A and Alt+Y remain aliases for File and Help.

## View and Options

View contains Split Window, Size Window, Close Window and the open document list, with the shortcuts shown above. Up to nine documents can remain open. New and Open preserve existing documents. Split Window creates a second view of the current document; each panel has its own cursor, selection and scroll position. Choose another document from View or Alt+1–9 to show it in the active panel. Switch panels with F6 or a mouse click. Edits to a shared document appear in both views. The clipboard is shared between documents; undo and redo belong to each document.

Size Window uses Up/Down or mouse movement with the left button held to move the divider; Enter accepts and Esc cancels. Closing one of two panels keeps its document open. Closing the sole panel closes its document, asking about unsaved changes; closing the last document creates a blank one. Quit checks every open document for unsaved changes. A terminal shorter than 12 rows returns to one panel.

Options contains Settings and Colors. Settings changes Tab Stops from 1 to 32 columns without changing file contents. DOS printer port choices are not included in this Linux editor. Colors provides Item, Foreground and Background lists plus a sample, Default, OK, Cancel and Help. It configures normal text, selections, window borders, the menu bar, status bar, keyboard accelerators and disabled items. Cancel discards the draft; Default restores the draft palette and OK applies it. A terminal advertising at least 16 colors displays the complete palette; eight-color terminals use the corresponding base colors.

Preferences are saved when accepted to `$XDG_CONFIG_HOME/gui/settings`, or `~/.config/gui/settings` when XDG_CONFIG_HOME is unset. If writing fails they still apply to the current session and the status bar reports the error. Corrupt preferences fall back to defaults.

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
make -B && make test
./gui sample.txt
```

Fedora dependencies: `gcc make ncurses-devel git`. Rust is not required.
After validating the prototype, `make install` installs to `/usr/local/bin` by default.

## Validation

Core tests cover selection, undo/redo, search, replacement, mouse coordinate mapping, UTF-8/CRLF file I/O, file browser directory scanning, read-only protection, binary hex views, multiple documents, clipboard transfer, split view cursor independence, tab spacing and preference persistence. Updated tests have not been run by the assistant.

On Linux, also check all selection shortcuts, menu activation (including Alt/F10), every dialog's appearance, split resizing and pane clicks, terminal resize, canceled exits with multiple unsaved documents, preference cancellation and failed saves. Some terminals intercept Ctrl+function keys; the corresponding View menu commands work without those key sequences. Compare the appearance against the classic editor references, particularly in KiTTY.

## Current scope

Up to nine documents, two panels and literal search without regular expressions. Text editing requires UTF-8 without NUL bytes; other files can be inspected using Open Binary, a read-only hex view. UTF-8 BOM and uniform LF/CRLF line endings are preserved. Mixed line endings are normalized to CRLF when CRLF is present. Saving over symbolic links is not supported.

The document and up to 64 undo states are stored as memory copies, so this prototype is intended for small and medium text files. Editing operates on Unicode code points; complex emoji and combining sequences are not yet treated as indivisible grapheme clusters.

MIT License. The About dialog identifies gui's own C edition and version.
