# gui — C edition

An independent Linux terminal editor written from scratch in C11 with ncursesw.
The `c-rewrite` branch contains new C code, not a translation of Microsoft Edit's Rust implementation. The older Rust variant remains in `main`.

**Development prototype. The latest interface changes have not been compiled or visually verified by the assistant.** Try this branch separately before replacing an installed editor.

## Classic appearance

- English interface: File, Edit, Search, View, Options and Help.
- Blue editing area with white text, a thin frame, and a centered file name over a black title strip.
- Light gray menu bar and bottom help bar with black text.
- No line numbers and no current-line highlight. Only explicitly selected text changes color.
- Gray dialogs with white title strips, dotted input fields, checkboxes and white buttons with half-cell black shadows.

The terminal font controls the exact appearance of characters and shadows.

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
| Quit | Ctrl+Q |
| Undo / Redo | Ctrl+Z / Ctrl+Y |
| File menu | Alt+A or Alt+F |
| Edit menu | Alt+E |
| Search menu | Alt+B or Alt+S |
| View / Options menus | Alt+V / Alt+O |
| Help menu | Alt+Y or Alt+H |
| Menu bar / About gui | F10 / F1 |

The clipboard is internal in this prototype. To paste from another application, use your terminal's paste command (usually Ctrl+Shift+V). Bracketed terminal paste is supported in the document. Some terminals intercept Ctrl+Insert or Shift+Insert.

Use Tab and Shift+Tab to move between dialog controls, Space to toggle checkboxes, Enter to activate a control, and Esc to cancel. The mouse can place the cursor, select text, open menus and activate search dialog controls.

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

Core tests cover selection, undo/redo, search, replacement, mouse coordinate mapping, and UTF-8/CRLF file I/O. Updated tests have not been run by the assistant.

On Linux, also check all selection shortcuts, menu activation, dialog focus, terminal resize, canceled exits with unsaved changes, and failed saves. Compare the appearance against the classic editor references.

## Current scope

One open document, literal search without regular expressions, and UTF-8 text files. Binary files, NUL bytes and invalid UTF-8 are rejected. UTF-8 BOM and uniform LF/CRLF line endings are preserved. Mixed line endings are normalized to CRLF when CRLF is present. Saving over symbolic links is not supported.

The document and up to 64 undo states are stored as memory copies, so this prototype is intended for small and medium text files. Editing operates on Unicode code points; complex emoji and combining sequences are not yet treated as indivisible grapheme clusters.

MIT License. The About dialog identifies gui's own C edition and version.

