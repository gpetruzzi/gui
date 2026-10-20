# gui 1.0.1

A Linux terminal text editor by **Guillermo Petruzzi**, with a classic DOS-style interface, keyboard shortcuts and mouse support.

Open up to nine documents, work in two editing panels, search and replace text, and customize the interface colors. gui has an English interface and is developed on the `main` branch.

## Classic appearance

- English interface: File, Edit, Search, View, Options and Help.
- Blue editing area with white text, a thin frame, and a centered file name along a thin white title rule.
- Light gray menu bar and bottom help bar with black text.
- The compact Edit menu contains Cut, Copy, Paste and Clear, with gray unavailable commands, a black selection bar and a contextual description in the bottom bar. Clear deletes only the selection and keeps the internal clipboard unchanged. Undo, Redo and Select All remain available through their keyboard shortcuts.
- No line numbers and no current-line highlight. Only explicitly selected text changes color.
- Gray dialogs with white title strips, dotted input fields, checkboxes and white buttons with thin black shadows. Labels highlight their Alt access letter in white; button access letters are underlined in black for contrast against the white button face.
- File and directory scrollbars use white arrows on black, white stipple on a black track and a solid white proportional thumb. These styles are shared by all applicable dialogs and editor panels.
- Open and Save As use a classic two-pane browser: File Name, current path, Existing Files and Directories, with OK, Cancel and Help buttons. Use Tab to move between controls and arrows to select an entry. In Open, Enter opens the selected file immediately; double-clicking a file also opens it. Enter on a directory navigates into it. Saving to another existing file asks before replacing it.
- Open has a taller Files pane and a shorter Directories pane, followed by Open Read-only, Open Binary and Line Width. Read-only blocks edits and saving. Open Binary opens a **read-only hexadecimal view**, not a binary editor; Line Width is automatic in binary view: the text uses the available terminal width.

The terminal font controls the exact appearance of characters and shadows. All dialogs share a full-width, one-row white title strip, white buttons with black text, inward ASCII arrow focus markers (`> OK <`) and a short half-cell black shadow offset down and right. On 256-color terminals, fixed palette slots provide black (#000000), white (#ffffff) and a darker gray (#a8a8a8). Button text uses no bold or reverse video. A UTF-8 font containing half-block glyphs is required for those details. No terminal palette entries are reprogrammed.

gui respects the connection's TERM and never substitutes a different terminfo profile. Borders, arrows and scrollbar stipple are written as Unicode characters through ncursesw, avoiding VT100 alternate-character-set sequences that can appear as q/x letters in UTF-8 terminal sessions. Real eight-color terminals cannot display separate light gray and bright white backgrounds. Configure KiTTY's actual terminal capabilities before advertising a 256-color profile.

The editor status bar shows the current one-based Line and Col at the right; columns account for tabs and wide characters. About gui stays open until Enter, Esc, Alt+O or a click on OK; unrelated mouse events do not dismiss it.

Use the arrow keys to navigate menus. Disabled commands can be highlighted but cannot be executed. In file browsers and the Colors dialog, selected list entries use white text on a black background.

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

The clipboard is internal to gui and shared between its open documents. To paste from another application, use your terminal's paste command (usually Ctrl+Shift+V). Bracketed terminal paste is supported in the document. Some terminals intercept Ctrl+Insert or Shift+Insert.

To copy with the mouse, hold the left button and drag, release it, press Ctrl+C or choose Edit → Copy, then use Ctrl+V inside gui. The status bar reports the number of characters copied to gui's internal clipboard. Copying without a selection reports "No text selected." Drag handling preserves its anchor across repeated held-button events and ends on release, including outside the document.

Use Tab and Shift+Tab to move between dialog controls, Space to toggle checkboxes, Enter to activate a control, and Esc to cancel. The mouse can place the cursor, select text, open menus and activate search dialog controls.

A single Esc cancels the current dialog, regardless of the focused control.
The default terminal escape-sequence delay is 50 milliseconds, with no extra
blocking wait in the dialog Alt-key parser. For connections that deliver
function-key sequences slowly, an explicit `ESCDELAY` setting is respected;
for example, `ESCDELAY=200 ./gui` allows a longer sequence timeout.

Dialog Alt shortcuts: file browsers use N for File Name, F for Files, D for Directories, O for OK, C for Cancel and H for Help. Open adds R for Read-only, B for Binary and W for Line Width. Find/Replace use F for Find What, R for Replace With, W for Whole Word, M for Match Case, O for OK, A for All, C for Cancel and H for Help. Save confirmations use S for Save, D for Discard and C for Cancel.

## Build and run

gui is written in C11 and requires a compiler, Make, Git and the ncursesw development library. Run package installation commands as root, or prefix them with `sudo`.

### Debian and Ubuntu

```sh
apt update
apt install build-essential libncurses-dev git
git clone --branch main --single-branch https://github.com/gpetruzzi/gui.git gui-c
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

### Fedora

Install the dependencies as root, then use the same clone and build commands above:

```sh
dnf install gcc make ncurses-devel git
```

### Install or update the installed executable

After building and testing, run as root:

```sh
make install
```

This installs gui in `/usr/local/bin`. Launch it with `gui` for a blank document or `gui filename.txt` to open a file. Use `./gui` when testing a fresh build in the source directory; repeat `make install` to update a previously installed copy.

Check the version with `./gui --version`. For gui 1.0, About shows:

```text
gui 1.0
Author: Guillermo Petruzzi
License: GNU GPLv3 or later
```

## Debian package candidate

The repository includes a Debian packaging candidate, maintained by
Guillermo Petruzzi <gpetruzzi@gmail.com>. It is not yet an official Debian
package. See [Debian packaging instructions](docs/debian-packaging.md) for
dependencies, local builds, Lintian checks and the path toward sponsorship.

After installing the documented build dependencies:

```sh
sh scripts/build-debian.sh
```

The script builds committed files in a separate temporary workspace and copies
artifacts to `dist/debian/`. The Debian package installs `/usr/bin/gui` and
the `gui(1)` manual; an older `/usr/local/bin/gui` is left untouched.

## Validation

`make test` runs the core tests and headless ncurses dialog regressions. The
dialog tests check direct opening from the file list, directory navigation,
read-only opening, cancellation of each modal with a single Esc, and Alt and
arrow-key handling. They use temporary streams and require xterm terminfo,
provided by Debian's ncurses-base package; no interactive display is needed.

Core tests cover selection, undo/redo, search, replacement, mouse coordinate mapping, UTF-8/CRLF file I/O, file browser directory scanning, read-only protection, binary hex views, multiple documents, clipboard transfer, split view cursor independence, tab spacing and preference persistence. Run them locally with `make test`; terminal appearance and input behavior also need checking in your terminal.

On Linux, also check all selection shortcuts, menu activation (including Alt/F10), every dialog's appearance, split resizing and pane clicks, terminal resize, canceled exits with multiple unsaved documents, preference cancellation and failed saves. Some terminals intercept Ctrl+function keys; the corresponding View menu commands work without those key sequences. Compare the appearance against the classic editor references, particularly in KiTTY.

## Current scope

Up to nine documents, two panels and literal search without regular expressions. Text editing requires UTF-8 without NUL bytes; other files can be inspected using Open Binary, a read-only hex view. UTF-8 BOM and uniform LF/CRLF line endings are preserved. Mixed line endings are normalized to CRLF when CRLF is present. Saving over symbolic links is not supported.

The document and up to 64 undo states are stored as memory copies, so gui is intended for small and medium text files. Editing operates on Unicode code points; complex emoji and combining sequences are not yet treated as indivisible grapheme clusters.

## Author

**Guillermo Petruzzi**

## License

gui is distributed under the **GNU General Public License, version 3 or (at your option) any later version** (SPDX: `GPL-3.0-or-later`). See [LICENSE](LICENSE) for the complete terms and [LICENSING.md](LICENSING.md) for the transition from MIT.

You may use, modify and redistribute gui, including commercially. When distributing covered versions, comply with the GPL requirements, including providing the corresponding source code and preserving the required notices. Private modifications do not require public release.

Previously published MIT versions retain their MIT permissions. The historical MIT notice is preserved in [LICENSES/MIT-legacy.txt](LICENSES/MIT-legacy.txt). External dependencies retain their own licenses.

The About dialog identifies gui 1.0, its author Guillermo Petruzzi, and the GPL license.

## Changes in 1.0.1

Opening a file removes untouched empty Untitled documents, including when
selecting a file that is already open. Documents containing text or edit
history are preserved. Failed opens keep the empty document.

## Binary viewer layout

Open Binary fills the available left pane with readable ASCII characters,
using dots for non-printable bytes. Source offsets appear at the left.
The right pane stays fixed at ten bytes per row (29 characters with spaces).
The panes wrap independently: ASCII uses the available width; hex uses ten
bytes. Moving the cursor scrolls the hexadecimal view to keep the same byte
visible and highlighted in both panes. Selecting in either pane highlights
the corresponding bytes. Clicking a hex byte moves to its source position.
Arrow keys and Page Up/Down move through the ASCII layout. Resize reflows
the text while preserving the byte position. The viewer opens read-only by default
and requires at least 53 terminal columns.

## Binary overwrite editing

Binary files still open read-only. Use Options -> Edit Binary or F4 to enable
overwrite editing. Tab switches between HEX and ASCII; clicking either pane
also chooses it. In HEX enter two digits (00-FF) to replace one byte; the first
digit is a preview and does not modify the file. Moving the cursor or changing
modes cancels that preview. ASCII mode accepts printable ASCII characters.
The cursor advances after a complete byte. File size never changes: insertion,
deletion, paste and replacement are unavailable in binary mode.

Ctrl+Z/Ctrl+Y undo and redo complete byte changes. Ctrl+S saves the original
byte sequence with the changes, including NUL and non-ASCII bytes, using the
regular atomic save path. Save As is also available. Unsaved binary changes
participate in the usual save/discard prompt. Save or undo changes before
switching back to read-only mode.
