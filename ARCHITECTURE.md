# VI Editor Architecture

This document describes the architecture of the Open Watcom VI editor, a full-featured
vi-clone written in C. It targets DOS, Windows (console and GUI), OS/2, QNX, and
Unix/Linux from a single source tree.

---

## Table of Contents

1. [High-Level Overview](#high-level-overview)
2. [Directory Layout](#directory-layout)
3. [Core Data Structures](#core-data-structures)
4. [Main Loop and Command Dispatch](#main-loop-and-command-dispatch)
5. [Editing Modes](#editing-modes)
6. [Operator-Motion Composition](#operator-motion-composition)
7. [File Control Blocks (FCBs)](#file-control-blocks-fcbs)
8. [Undo/Redo System](#undoredo-system)
9. [Save Buffers (Registers)](#save-buffers-registers)
10. [Marks](#marks)
11. [Dot Command (Repeat)](#dot-command-repeat)
12. [Script Engine](#script-engine)
13. [Syntax Highlighting](#syntax-highlighting)
14. [Search and Regular Expressions](#search-and-regular-expressions)
15. [Ex Commands](#ex-commands)
16. [Display and Windowing](#display-and-windowing)
17. [Platform Abstraction](#platform-abstraction)
18. [Build System](#build-system)
19. [The Bind Tool (edbind)](#the-bind-tool-edbind)
20. [Configuration and Data Files](#configuration-and-data-files)
21. [Key Source File Reference](#key-source-file-reference)

---

## High-Level Overview

The editor is organized into four layers:

```
+--------------------------------------------------+
|              Platform-Specific Main               |
|        (nt/ntmain.c, unix/unixmain.c, ...)       |
+--------------------------------------------------+
|           UI Layer (character or GUI)             |
|    ui/*.c (char-mode)  |  win/*.c (Windows GUI)  |
+--------------------------------------------------+
|              Core Editor Engine                   |
|                    c/*.c                          |
+--------------------------------------------------+
|            BIOS / Platform Abstraction            |
|  nt/biosnt.c  linux/bioslinux.c  dos/biosdos.c   |
+--------------------------------------------------+
```

- **Core engine** (`c/`): ~105 source files implementing all editing logic, file
  management, undo, scripting, syntax highlighting, search, and ex commands.
- **Platform layer** (`nt/`, `unix/`, `linux/`, `dos/`, `os2/`, `qnx/`): OS-specific
  keyboard, screen, file system, and process APIs.
- **UI layer** (`ui/`): Character-mode windowing with borders, scrollbars, menus,
  and overlapping windows. Alternatively, `win/` provides a full Windows GUI.
- **Headers** (`h/`): ~49 header files defining all types, constants, and interfaces.

---

## Directory Layout

```
vi/
  c/            Core editor source (~105 .c files)
  h/            Header files (~49 .h files)
  ui/           Character-mode UI (windows, borders, menus, scrollbars)
  win/          Windows GUI (native Win32 windowed application)
  nt/           Windows NT console platform layer
  unix/         Generic Unix platform layer
  linux/        Linux platform layer (ncurses-based)
  dos/          DOS platform layer
  os2/          OS/2 platform layer
  qnx/          QNX platform layer
  curses/       ncurses wrapper library
  noui/         Headless/minimal UI fallback
  dat/          Configuration, syntax definitions, scripts (.cfg, .dat, .vi)
  doc/          Help files (.hlp)
  mif/          Watcom make include files (build system)
  ctl/          UI control/dialog definitions and parsers
  ctags/        ctags utility source
  bind/         edbind utility (appends data to executables)
  msvc/         Visual Studio project files and MSVC compatibility stubs
  man/          Man page source
  dos386/       DOS 32-bit build output directory
  dosi86/       DOS 16-bit build output directory
  nt386/        NT x86 console build output directory
  nt386.win/    NT x86 GUI build output directory
  ntaxp/        NT Alpha console build output directory
  ntaxp.win/    NT Alpha GUI build output directory
  linux386/     Linux x86 build output directory
  linuxmps/     Linux MIPS build output directory
  linuxppc/     Linux PowerPC build output directory
  os2386/       OS/2 32-bit build output directory
  os2i86/       OS/2 16-bit build output directory
  qnxi86/       QNX build output directory
  win386/       Windows 3.x 32-bit build output directory
  wini86/       Windows 3.x 16-bit build output directory
  files.dat     ctags file list
  lang.ctl      Top-level build control file
  prereq.ctl    Prerequisite build control (builds edbind and ctl parsers first)
  vi.msg        UI message strings (pick macros)
  tooltips.msg  Toolbar tooltip strings
```

---

## Core Data Structures

All major structures are defined in `h/struct.h`. The type system is built around
these key abstractions:

### line

A single line of text, stored as a doubly-linked list node:

```c
typedef struct line {
    struct line     *next, *prev;   // linked list
    short           len;            // data length
    linedata        inf;            // flags: mark, global match, hidden, hilite
    char            data[1];        // variable-length character data
} line;
```

### fcb (File Control Block)

A chunk of lines within a file. FCBs are the fundamental unit of memory management.
Lines are grouped into FCBs which can be swapped to disk, EMS, XMS, or extended memory:

```c
typedef struct fcb {
    struct fcb      *next, *prev;       // per-file chain
    struct fcb      *thread_next, *thread_prev;  // global thread (all FCBs)
    line_list       lines;              // head/tail of lines in this FCB
    linenum         start_line, end_line;
    long            byte_cnt, offset;   // byte count, swap file offset
    // Status flags:
    unsigned        swapped     :1;     // data is on disk/EMS/XMS
    unsigned        in_memory   :1;     // data is in RAM
    unsigned        on_display  :1;     // currently visible
    unsigned        non_swappable :1;
    unsigned        in_extended_memory :1;
    unsigned        in_xms_memory :1;
    unsigned        in_ems_memory :1;
    // ...
} fcb;
```

### file

Represents an open file (one or more FCBs):

```c
typedef struct file {
    char            *name, *home;
    fcb_list        fcbs;           // chain of FCBs
    long            size;
    bool            modified, read_only, viewonly;
    bool            check_readonly, dup_count, been_autosaved;
    int             handle;
} file;
```

### info

Complete editing session state for one file. When you switch between files, the
editor saves and restores `info` structures:

```c
typedef struct info {
    struct info     *next, *prev;       // linked list of all open files
    file            *CurrentFile;
    i_mark          CurrentPos, LeftTopPos;
    undo_stack      *UndoStack, *UndoUndoStack;
    int             CurrentUndoItem, CurrentUndoUndoItem;
    window_id       current_window_id;
    mark            *MarkList;
    select_rgn      SelRgn;
    // Flags: linenumflag, CMode, WriteCRLF, etc.
    // Display cache, tab settings, language info, ...
} info;
```

### event

Maps a keystroke to handler functions with behavior flags:

```c
typedef struct event {
    command_rtn     rtn;            // union of function pointers
    event_bits      b;              // type + behavioral flags
    command_rtn     alt_rtn;        // alternate (modeless) handler
    event_bits      alt_b;
} event;
```

The `command_rtn` union holds typed function pointers for different event categories:

```c
typedef union command_rtn {
    insert_rtn      ins;            // insert mode handler
    move_rtn        move;           // movement (returns range)
    op_rtn          op;             // operator (receives range)
    misc_rtn        misc;           // miscellaneous command
    old_rtn         old;            // legacy handler
    alias_rtn       alias;          // aliased command
} command_rtn;
```

### Other Key Structures

| Structure     | Purpose |
|---------------|---------|
| `i_mark`      | Position marker (line number + column) |
| `mark`        | Named bookmark (a-z) with position |
| `range`       | Selection range with start/end positions and flags |
| `select_rgn`  | Current visual selection state |
| `savebuf`     | Register/yank buffer (character or FCB-based) |
| `undo`        | Single undo action with type and data union |
| `undo_stack`  | Stack of undo records with depth tracking |
| `wind`        | Window structure (position, size, colors, borders) |
| `ss_block`    | Syntax highlighting block (type, position, length) |
| `dc_line`     | Display cache line (cached rendered text + syntax info) |
| `regexp`      | Compiled regular expression with sub-expression matches |
| `key_map`     | Keyboard mapping (key sequence to command expansion) |
| `sfile`       | Script file entry (parsed script command) |
| `vars`        | Script variable (name/value pair in linked list) |
| `lang_info`   | Language definition (keyword hash table for highlighting) |
| `fancy_find`  | Find/replace dialog state |
| `eflags`      | 50+ boolean flags tracking global editor state |

---

## Main Loop and Command Dispatch

The main loop lives in `c/editmain.c`:

```
EditMain()
  loop forever:
    1. Update display (DCUpdateAll or DCUpdate)
    2. Update status line
    3. LastEvent = GetNextEvent(TRUE)     // read keystroke
    4. Clear message window
    5. rc = DoLastEvent()                 // dispatch
    6. DoneLastEvent(rc, FALSE)           // post-command cleanup
    7. Display error if rc > ERR_NO_ERR
```

### DoLastEvent() Dispatch

Looks up the keystroke in the global `EventList[]` array and routes by type:

| Event Type        | Action |
|-------------------|--------|
| `EVENT_INS`       | Call `event->rtn.ins()` directly (insert-mode operation) |
| `EVENT_OP`        | Call `doOperator()` — waits for motion, computes range, calls operator |
| `EVENT_REL_MOVE`  | Call `DoMove()` — executes motion, updates cursor |
| `EVENT_ABS_MOVE`  | Call `DoMove()` — executes motion, updates cursor |
| `EVENT_MISC`      | Call `event->rtn.misc()` directly |

### DoneLastEvent() Cleanup

After each command:
- Saves the command into the dot buffer if it was "dotable"
- Resets the repeat count
- Resets savebuf selection

---

## Editing Modes

The editor supports these modes, tracked by flags in the global `EditFlags` struct:

| Mode | Flag | Description |
|------|------|-------------|
| **Command** | default | Normal vi command mode |
| **Insert** | `InsertModeActive` | Text insertion (i, a, o, O, etc.) |
| **Replace** | `InsertModeActive` + overstrike | Overstrike mode (R) |
| **Ex** | `ExMode` | Line-oriented command mode (:) |
| **Modeless** | `Modeless` | Non-modal editing (always inserting) |
| **Key Map** | `KeyMapMode` | Executing a mapped key sequence |
| **Visual** | `SelRgn.selected` | Visual selection active |

### Insert Mode (`c/editins.c`)

Key functions:
- `InsertTextAtCursor()`, `InsertTextAfterCursor()`, `InsertTextAtLineStart()`,
  `InsertTextAtLineEnd()`, `OpenLineAbove()`, `OpenLineBelow()` — entry points
- `IMChar()` — insert a character
- `IMEnter()` — split line with auto-indent
- `IMBackSpace()`, `IMDelete()` — character deletion
- `IMTabs()` — tab/shift-tab handling (real tabs or spaces)
- `IMCloseBrace()`, `IMCloseBracket()` — auto-formatting in C mode
- `PushMode()` / `PopMode()` — save/restore insert state for nested operations

---

## Operator-Motion Composition

The vi-style `operator + motion` system is implemented in `doOperator()` within
`c/editmain.c`. When an operator key is pressed (d, c, y, >, <, !, gu, gU):

1. Read the next event (the motion or text object)
2. Handle doubled operators (e.g., `dd` = delete line, `yy` = yank line)
3. Handle special cases (`cw` treats word boundary differently than `ce`)
4. Execute the motion function to compute a `range` (start + end positions)
5. Normalize the range (ensure start <= end)
6. Call the operator function with the range

g-prefix operators (`gu`, `gU`) are handled by `doGOperator()` in `c/gcmd.c`,
which follows the same pattern.

### Vim Text Objects

The editor supports vim-like text objects (e.g., `iw`, `aw`, `i"`, `a(`).
These are handled during operator dispatch — when the motion keystroke is `i` or `a`,
the next character determines the text object type.

---

## File Control Blocks (FCBs)

FCBs are the core memory management abstraction, allowing the editor to handle files
larger than available RAM. This was critical for the DOS/16-bit era targets.

### FCB Lifecycle

```
File Load → Split into FCBs → Lines in memory
                ↓ (memory pressure)
            Swap to disk/EMS/XMS
                ↓ (needed again)
            Swap back into memory
```

### FCB Source Files

| File | Purpose |
|------|---------|
| `c/fcb.c` | Allocation, freeing, global FCB thread management |
| `c/fcbmem.c` | Memory allocation/deallocation for FCB contents |
| `c/fcbblock.c` | Block-level FCB operations |
| `c/fcbdisk.c` | Swap FCBs to/from disk (swap file) |
| `c/fcbems.c` | Swap FCBs to/from EMS (Expanded Memory) |
| `c/fcbxms.c` | Swap FCBs to/from XMS (Extended Memory) |
| `c/fcbxmem.c` | Swap FCBs to/from extended memory |
| `c/fcbxmint.c` | Extended memory interrupt handler |
| `c/fcbswap.c` | High-level swap orchestration |
| `c/fcbsplit.c` | Split an FCB when it exceeds capacity |
| `c/fcbmerge.c` | Merge adjacent small FCBs |
| `c/fcbdup.c` | Duplicate an FCB and its lines |
| `c/fcbdmp.c` | Debug: dump FCB state and heap info |

### Key Operations

- **FetchFcb()**: Ensure an FCB's lines are in memory (swap in if needed)
- **SwapFcb()**: Move an FCB's data out of main memory
- **SplitFcb()**: Split when too many lines accumulate in one FCB
- **MergeFcbs()**: Combine adjacent small FCBs to reduce overhead

---

## Undo/Redo System

The undo system uses grouped transactions with position tracking.

### Source Files

| File | Purpose |
|------|---------|
| `c/undo.c` | Core undo operations (start/end groups, record changes) |
| `c/undostks.c` | Undo stack management |
| `c/undo_do.c` | Execute undo/redo operations |
| `c/undoclne.c` | Undo for current-line edits |

### How It Works

1. **StartUndoGroup()** — begin a transaction, save cursor position
2. Editing operations record individual undo items:
   - `UndoReplaceLines()` — line replacement
   - `UndoDeleteFcbs()` — FCB deletion
   - `UndoInsert()` — line insertion
3. **EndUndoGroup()** — close the transaction

Undo items are pushed onto `UndoStack`. When undone, they move to `UndoUndoStack`
(the redo stack). Groups can nest via an `OpenUndo` depth counter.

---

## Save Buffers (Registers)

Save buffers are the vi equivalent of clipboard registers (`c/savebuf.c`):

- **9 numbered registers** (`1`-`9`): Auto-rotate on delete (newest at 1)
- **26 named registers** (`a`-`z`): User-specified
- **1 unnamed register** (`""`): Default target
- **Special registers**: Clipboard integration on Windows

Each `savebuf` holds either raw character data or a list of FCBs (for multi-line
content). `rotateSavebufs()` shifts numbered registers on each delete operation.

---

## Marks

Marks (`c/mark.c`) store named positions in the file:

- **a-z**: User-set marks (per-file)
- **`.` mark**: Used internally for memorize/dot-command recording

`SetMark()` prompts for a letter and stores the current line/column.
Marks are stored as a linked list on each `info` structure and reference
specific lines.

---

## Dot Command (Repeat)

The dot (`.`) command replays the last editing command (`c/dotmode.c`):

- `SaveDotCmd()` records keystrokes into `DotBuffer` after each "dotable" command
- The `DotMode` flag prevents recursive recording during replay
- `DotDigits` and `DotCount` manage repeat count state
- An alternate dot buffer (`=`) provides a second memorize slot

---

## Script Engine

The editor includes a full scripting language for automation and configuration.

### Source Files

| File | Purpose |
|------|---------|
| `c/source.c` | Main script driver (load and execute .vi files) |
| `c/srcgen.c` | Script code generation and execution |
| `c/srcassgn.c` | Variable assignment with expansion (regex, env, time) |
| `c/srcvar.c` | Variable management (global variables, row/column vars) |
| `c/srcexpnd.c` | Variable expansion in script lines |
| `c/srcif.c` | IF/ELSEIF/ELSE/ENDIF conditional processing |
| `c/srccs.c` | Control structure management (block nesting) |
| `c/srcgoto.c` | GOTO/label processing |
| `c/srchook.c` | Hook routines (event-driven script callbacks) |
| `c/srcfile.c` | Script file I/O operations |
| `c/srcexpr.c` | Expression evaluation in scripts |
| `c/srcinp.c` | Script input handling |
| `c/srcnextw.c` | Next-word parsing for script tokens |
| `c/srcdata.c` | Script token data definitions |

### Script Features

- **Control flow**: `if`/`elseif`/`else`/`endif`, `while`/`endwhile`,
  `loop`/`endloop`, `goto`/labels, `break`, `continue`
- **Variables**: Global variables, `%E` (environment), `%D` (date/time),
  `%R` (regex matches), row/column variables
- **Expression evaluation**: Arithmetic, bitwise, logical, and comparison operators
  (`c/expr.c`)
- **Hooks**: Scripts can register callbacks for editor events (read, write, modify,
  file open, etc.)
- **File I/O**: Open, read, write, close files from scripts
- **Compilation**: Scripts can be pre-compiled to `resident` structures for faster
  re-execution

### Configuration

The main configuration file is `dat/ed.cfg`, which is itself a script executed at
startup. It defines menus, key mappings, colors, hooks, and file-type associations.

---

## Syntax Highlighting

The syntax highlighting system provides language-aware colorization.

### Source Files

| File | Purpose |
|------|---------|
| `c/sstyle.c` | Main syntax style controller |
| `c/sstyle_c.c` | C/C++ syntax rules |
| `c/sstyle_f.c` | Fortran syntax rules |
| `c/sstyle_h.c` | HTML/markup syntax rules |
| `c/sstyle_g.c` | GML syntax rules |
| `c/sstyle_m.c` | Makefile syntax rules |
| `c/sstyle_p.c` | Perl syntax rules |
| `c/lang.c` | Language definition loading (keyword hash tables from .dat files) |

### How It Works

1. Language is determined by file extension via `filetypesource` configuration
2. Keyword lists are loaded from `.dat` files (e.g., `dat/c.dat`, `dat/java.dat`)
   into a hash table (`lang_info`)
3. The `ss_block` structure tags each region of a line with a `syntax_element` type:
   `TEXT`, `WHITESPACE`, `KEYWORD`, `OCTAL`, `HEX`, `INTEGER`, `FLOAT`, `CHAR`,
   `STRING`, `COMMENT`, `PREPROCESSOR`, `SYMBOL`, `INVALIDTEXT`, etc.
4. The display cache (`dc_line`) stores pre-computed syntax blocks per line
5. Colors for each element type are configurable

### Supported Languages

C, C++, Java, JavaScript, TypeScript, C#, Perl, SQL, Fortran, AWK, BASIC, HTML,
GML, WML, XML, JSON, shell/batch, Makefile, MIF, and more (via `.dat` files in
`dat/`).

---

## Search and Regular Expressions

### Source Files

| File | Purpose |
|------|---------|
| `c/findcmd.c` | Find command handlers (forward, backward, fancy dialog) |
| `c/findrx.c` | Regex search with wrap-around |
| `c/grep.c` | Multi-file grep with results window |
| `c/match.c` | Bracket/brace/parenthesis matching |
| `c/fmatch.c` | Filename wildcard matching |
| `c/rxwrap.c` | Regex wrapper functions |
| `c/rxsupp.c` | Regex support utilities |
| `c/regsub.c` | Regex substitution (backreferences \1-\9) |

### Regular Expression Engine

The regex engine (`h/regexp.h`) supports:
- Sub-expression capture (`\(` ... `\)`) with up to 10 groups
- Anchoring (`^`, `$`)
- Character classes (`[...]`)
- Quantifiers (`*`, `\+`, `\?`)
- Backreferences in substitution (`\1` through `\9`)
- Case sensitivity control (global, per-search, or per-regex)

---

## Ex Commands

Ex-mode commands (`:` commands) are implemented in `c/ex.c` and `c/exdata.c`:

### Command Categories

| Commands | Implementation |
|----------|---------------|
| `:w`, `:wq`, `:q`, `:q!` | File save/quit |
| `:e`, `:edit` | Open file |
| `:s`, `:substitute` | Find/replace (`c/clsubs.c`) |
| `:g`, `:global` | Global command on matching lines (`c/clglob.c`) |
| `:d`, `:delete` | Delete lines |
| `:y`, `:yank` | Yank lines |
| `:p`, `:put` | Put (paste) register contents |
| `:m`, `:move` | Move lines |
| `:co`, `:copy` | Copy lines |
| `:j`, `:join` | Join lines |
| `:r`, `:read` | Read file into buffer |
| `:!` | Shell command (`c/filter.c`) |
| `:set` | Set options (`c/clset.c`) |
| `:map`, `:unmap` | Key mapping (`c/mapkey.c`) |
| `:ab`, `:abbreviate` | Abbreviations (`c/alias.c`) |
| `:tag` | Navigate to tag (`c/tags.c`) |
| `:source` | Execute script file (`c/source.c`) |
| `:cd` | Change directory (`c/dir.c`) |

The `exdata.c` file defines the token table mapping command names to handler
functions and token IDs.

---

## Display and Windowing

### Display Cache (`c/dc.c`)

The display cache (`dc_line` array) avoids redundant screen redraws:
- Each line has a `display` flag (needs redraw) and `valid` flag (matches screen)
- Syntax highlighting blocks are cached per line
- Scroll operations shift the cache rather than recomputing everything
- `DCUpdateAll()` and `DCUpdate()` are the main refresh entry points

### Character-Mode UI (`ui/`)

The `ui/` directory implements a windowing system for terminal/console environments:

| File | Purpose |
|------|---------|
| `ui/winnew.c` | Create and destroy windows |
| `ui/windisp.c` | Window display and text rendering |
| `ui/winsize.c` | Window sizing and positioning |
| `ui/winswap.c` | Window content save/restore (for overlapping) |
| `ui/winbrdr.c` | Window borders with configurable characters |
| `ui/winthumb.c` | Scrollbar thumbs/gadgets |
| `ui/winover.c` | Overlapping window management |
| `ui/menu.c` | Menu system |
| `ui/uiledit.c` | Line editing within UI |
| `ui/uimisc.c` | Miscellaneous UI utilities |

Windows are managed via the `wind` structure with:
- Position (x1, y1, x2, y2), colors (border, text, highlight)
- Optional borders, scrollbar gadgets
- Overlap tracking for proper repaint ordering

### Windows GUI (`win/`)

The `win/` directory provides a full native Windows application (~21 .c files):
- MDI (Multiple Document Interface) window management
- Native menus, toolbars, status bars
- Font selection, color picker dialogs
- DDE (Dynamic Data Exchange) support
- OLE2 clipboard integration
- Print support
- Resource files (icons, bitmaps, cursors, dialogs)

---

## Platform Abstraction

### BIOS Interface (`h/vibios.h`)

Each platform implements a standard set of functions:

```c
void BIOSSetCursor(int row, int col);
void BIOSGetCursor(int *row, int *col);
int  BIOSGetKeyboard(int *scan);
int  BIOSTestKeyboard(void);
void BIOSUpdateScreen(size_t offset, unsigned count);
```

### Platform Implementations

| Directory | Platform | Key Files |
|-----------|----------|-----------|
| `nt/` | Windows NT console | `biosnt.c`, `ntsys.c`, `ntmain.c` (Win32 Console API) |
| `unix/` | Generic Unix | `biosunix.c`, `unixsys.c`, `unixmain.c` |
| `linux/` | Linux | `bioslinux.c` (ncurses), `linuxsys.c` |
| `dos/` | DOS | `biosdos.c`, `dossys.c` (BIOS interrupts) |
| `os2/` | OS/2 | `biosos2.c`, `os2sys.c` (Vio/Kbd/Mou APIs) |
| `qnx/` | QNX | `biosqnx.c`, `qnxsys.c` |
| `win/` | Windows GUI | Direct Win32 GDI (bypasses BIOS layer) |

### Compile-Time Selection (`h/control.h`)

Platform detection via preprocessor macros (`__WIN__`, `__NT__`, `__UNIX__`,
`__OS2__`, `__386__`, `__286__`). The `_FAR` macro abstracts far/near pointer
differences for 16-bit targets.

---

## Build System

### Watcom Make (Original)

The original build uses Open Watcom's `wmake` with `.mif` include files:

| File | Purpose |
|------|---------|
| `mif/master.mif` | Top-level: platform detection, tool paths, build orchestration |
| `mif/compile.mif` | Compiler flags per platform |
| `mif/objects.mif` | Object file lists (grouped by optimization priority) |
| `mif/link.mif` | Linker flags per platform |
| `mif/include.mif` | Include paths |
| `mif/special.mif` | Special build targets |
| `lang.ctl` | Top-level build control (orchestrates all platform builds) |
| `prereq.ctl` | Prerequisites (builds edbind and ctl parsers first) |

Build targets: `dos386`, `dosi86`, `nt386`, `nt386.win`, `ntaxp`, `ntaxp.win`,
`linux386`, `linuxmps`, `os2386`, `os2i86`, `qnxi86`, `win386`, `wini86`.

Each target directory contains a `makefile` that includes the shared `.mif` files
and sets platform-specific variables.

### MSVC (Modern)

The `msvc/` directory contains Visual Studio solution and project files:

- `vi.sln` — Solution file
- `vi.vcxproj` — Console build (NT character mode)
- `edbind.vcxproj` — edbind utility build
- Compatibility stubs: `getopt.c`, `autoenv.c`, `regexp.c`
- Compatibility headers: `banner.h`, `clibext.h`, `dos.h`, `getopt.h`, `bool.h`,
  `walloca.h`

Supports Debug/Release configurations for both Win32 and x64. Sources are pulled
from `c/`, `nt/`, and `ui/` directories with MSVC-specific stubs filling in
Watcom runtime functions.

---

## The Bind Tool (edbind)

`edbind` (`bind/main.c`) appends editor data files to the end of the executable,
enabling single-file distribution without external data directories.

### How It Works

1. Reads a list of data files from `dat/edbind.dat` (or `dat/winbind.dat` for GUI)
2. Concatenates them with length headers
3. Appends to the executable with a trailer:

```
[executable data] [bound data...] [data length: 4 bytes] [padding: 1 byte] ["CGEXXX": 6 bytes]
```

4. At runtime, `c/bnddata.c` reads the bound data by seeking to the end of the
   executable and checking for the `"CGEXXX"` magic cookie

### Usage

```
edbind [-q] [-s] [-d<datfile>] <executable>
    -q          Quiet mode
    -s          Strip bound data from executable
    -d<file>    Specify custom data file list
```

The `ed.cfg` startup script is always the first file in the bind list, so it
executes automatically on startup.

---

## Configuration and Data Files

### Startup Configuration (`dat/ed.cfg`)

The main configuration script, executed at editor startup. Defines:
- Menu structure and keyboard shortcuts
- Window colors and dimensions
- File type associations (`filetypesource`)
- Read/write hooks
- Key mappings (`:map` commands)
- Editor option defaults (`:set` commands)

### Syntax Definition Files (`dat/*.dat`)

Keyword lists for syntax highlighting, one per language:
`c.dat`, `cpp.dat`, `java.dat`, `javascript.dat`, `typescript.dat`, `csharp.dat`,
`perl.dat`, `sql.dat`, `fortran.dat`, `awk.dat`, `basic.dat`, `html.dat`,
`gml.dat`, `wml.dat`, `xml.dat`, `json.dat`, `bat.dat`, `mif.dat`, `rc.dat`

### Script Files (`dat/*.vi`)

Editor scripts for menus, dialogs, RCS integration, and utilities:
- `menu.vi`, `menuwin.vi` — Menu definitions
- `rcs.vi`, `chkout.vi`, `unlock.vi` — Version control hooks
- `qall.vi` — Quit all files
- `lnum.vi` — Toggle line numbering
- `rdme.vi`, `wrme.vi` — Read/write hooks

### Help Files (`doc/*.hlp`)

| File | Content |
|------|---------|
| `doc/start.hlp` | Startup, command-line options, edbind usage |
| `doc/cmd.hlp` | Command reference, address ranges |
| `doc/key.hlp` | Keystroke reference, key bindings |
| `doc/set.hlp` | Settings and options reference |
| `doc/script.hlp` | Scripting language reference |
| `doc/regexp.hlp` | Regular expression syntax |

### Windows Configuration (`dat/weditor.ini`)

Extended configuration for the Windows GUI build with color schemes, font
definitions, and window layout.

---

## Key Source File Reference

### Core Editing

| File | Purpose |
|------|---------|
| `c/editmain.c` | Main loop, event dispatch, operator composition |
| `c/editins.c` | Insert/replace mode |
| `c/editmv.c` | Page/screen scrolling, position commands |
| `c/editdc.c` | Delete/change on current line |
| `c/change.c` | Change/substitute command entry points |
| `c/delete.c` | Delete region and character operations |
| `c/move.c` | Cursor movement commands |
| `c/opmove.c` | Operator-motion helpers |
| `c/word.c` | Word motion definitions |
| `c/gcmd.c` | g-prefix commands (gu, gU) |

### File Management

| File | Purpose |
|------|---------|
| `c/file.c` | Save/restore editing context per file |
| `c/filenew.c` | Open new files |
| `c/filesave.c` | Write files to disk |
| `c/filemove.c` | Navigate between open files |
| `c/filelast.c` | Recent files list |
| `c/filestk.c` | File stack (push/pop) |

### Line Operations

| File | Purpose |
|------|---------|
| `c/linecfb.c` | Line-FCB interaction |
| `c/linedisp.c` | Line display rendering |
| `c/linedel.c` | Line deletion |
| `c/lineins.c` | Line insertion |
| `c/lineyank.c` | Line yanking |
| `c/linefind.c` | Find line by number |
| `c/linemisc.c` | Line utilities (join, find start) |
| `c/linenum.c` | Line number display |
| `c/lineptr.c` | Line pointer navigation |
| `c/linenew.c` | Create new lines |
| `c/linework.c` | Working line management |

### Command Processing

| File | Purpose |
|------|---------|
| `c/cmdline.c` | Command line prompt and execution |
| `c/ex.c` | Ex-mode commands |
| `c/exappend.c` | Ex append/insert/change commands |
| `c/exdata.c` | Ex command token table |
| `c/parsecl.c` | Command line parsing |
| `c/clset.c` | `:set` command processing |
| `c/clsubs.c` | `:substitute` command |
| `c/clglob.c` | `:global` command |
| `c/cledit.c` | Command-line file editing |
| `c/clread.c` | Command-line file reading |

### State and Globals

| File | Purpose |
|------|---------|
| `c/globals.c` | All global variable declarations |
| `c/init.c` | Editor initialization sequence |
| `c/fini.c` | Editor shutdown and cleanup |
| `c/dat.c` | Data file reader |
| `c/bnddata.c` | Read data bound into executable |

### Utilities

| File | Purpose |
|------|---------|
| `c/mem.c` | Memory allocation (MemAlloc, MemFree) |
| `c/error.c` | Error message retrieval and display |
| `c/hist.c` | Command/search/file history |
| `c/help.c` | Help system |
| `c/status.c` | Status line display |
| `c/cstatus.c` | Current mode status tracking |
| `c/io.c` | Low-level I/O utilities |
| `c/printf.c` | Formatted output |
| `c/misc.c` | Miscellaneous utilities |
| `c/readstr.c` | String reading from user |
| `c/abandon.c` | Fatal error abort |

---

## Global State

The editor's global state is centralized in `c/globals.c`:

- `EventList[]` — Keystroke-to-handler mapping table
- `CurrentFile`, `CurrentFcb`, `CurrentLine` — Current editing position
- `CurrentPos`, `LeftTopPos` — Cursor and viewport position
- `WorkLine` — Temporary buffer for in-progress line edits
- `Savebufs[]`, `SpecialSavebufs[]` — Registers (9 numbered + 26 named + 1 unnamed)
- `UndoStack`, `UndoUndoStack` — Undo and redo stacks
- `EditFlags` — 50+ boolean flags tracking editor state
- `KeyMaps[]`, `InputKeyMaps[]` — Normal and insert mode key mappings
- `InfoHead`, `InfoTail` — Linked list of all open file sessions
- `EditVars` — Configurable editor variables (tabstop, shiftwidth, etc.)
