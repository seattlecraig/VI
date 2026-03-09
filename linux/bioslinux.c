/****************************************************************************
*
* Description:  BIOS-style functions for Linux using ncurses.
*
*               Provides keyboard input, screen update, and cursor
*               positioning — the same interface as nt/biosnt.c but
*               implemented via ncurses instead of Win32 Console API.
*
****************************************************************************/

#include "vi.h"
#include <ncurses.h>
#include "win.h"
#include "vibios.h"

extern int PageCnt;

/* Color palette stubs — not applicable on terminals */
void BIOSGetColorPalette( void *a ) { (void)a; }
long BIOSGetColorRegister( int a ) { (void)a; return( 0 ); }
void BIOSSetNoBlinkAttr( void ) {}
void BIOSSetBlinkAttr( void ) {}
void BIOSSetColorRegister( int reg, char r, char g, char b )
{
    (void)reg; (void)r; (void)g; (void)b;
}

/*
 * BIOSSetCursor - position the cursor on screen
 */
void BIOSSetCursor( int page, int row, int col )
{
    (void)page;
    move( row, col );
    refresh();
}

/*
 * BIOSGetCursor - get the current cursor position.
 * Returns (row << 8) | col packed into a short.
 */
int BIOSGetCursor( char page )
{
    int     row, col;

    (void)page;
    getyx( stdscr, row, col );
    return( (row << 8) | col );
}

/*
 * Keyboard mapping table: maps ncurses KEY_* constants to vi_key values.
 */
struct keymap {
    int         nckey;      /* ncurses key constant */
    vi_key      vikey;      /* editor's vi_key value */
};

static struct keymap keymaps[] = {
    { KEY_UP,           VI_KEY( UP )            },
    { KEY_DOWN,         VI_KEY( DOWN )          },
    { KEY_LEFT,         VI_KEY( LEFT )          },
    { KEY_RIGHT,        VI_KEY( RIGHT )         },
    { KEY_HOME,         VI_KEY( HOME )          },
    { KEY_END,          VI_KEY( END )           },
    { KEY_PPAGE,        VI_KEY( PAGEUP )        },
    { KEY_NPAGE,        VI_KEY( PAGEDOWN )      },
    { KEY_IC,           VI_KEY( INS )           },
    { KEY_DC,           VI_KEY( DEL )           },
    { KEY_BACKSPACE,    VI_KEY( BS )            },
    { 127,              VI_KEY( BS )            },  /* some terminals */
    { KEY_ENTER,        VI_KEY( ENTER )         },
    { '\n',             VI_KEY( ENTER )         },
    { '\r',             VI_KEY( ENTER )         },
    { '\t',             VI_KEY( TAB )           },
    { 27,               VI_KEY( ESC )           },
    { KEY_F(1),         VI_KEY( F1 )            },
    { KEY_F(2),         VI_KEY( F2 )            },
    { KEY_F(3),         VI_KEY( F3 )            },
    { KEY_F(4),         VI_KEY( F4 )            },
    { KEY_F(5),         VI_KEY( F5 )            },
    { KEY_F(6),         VI_KEY( F6 )            },
    { KEY_F(7),         VI_KEY( F7 )            },
    { KEY_F(8),         VI_KEY( F8 )            },
    { KEY_F(9),         VI_KEY( F9 )            },
    { KEY_F(10),        VI_KEY( F10 )           },
    { KEY_F(11),        VI_KEY( F11 )           },
    { KEY_F(12),        VI_KEY( F12 )           },
    /* Shift+function keys: F13-F24 in ncurses */
    { KEY_F(13),        VI_KEY( SHIFT_F1 )      },
    { KEY_F(14),        VI_KEY( SHIFT_F2 )      },
    { KEY_F(15),        VI_KEY( SHIFT_F3 )      },
    { KEY_F(16),        VI_KEY( SHIFT_F4 )      },
    { KEY_F(17),        VI_KEY( SHIFT_F5 )      },
    { KEY_F(18),        VI_KEY( SHIFT_F6 )      },
    { KEY_F(19),        VI_KEY( SHIFT_F7 )      },
    { KEY_F(20),        VI_KEY( SHIFT_F8 )      },
    { KEY_F(21),        VI_KEY( SHIFT_F9 )      },
    { KEY_F(22),        VI_KEY( SHIFT_F10 )     },
    { KEY_F(23),        VI_KEY( SHIFT_F11 )     },
    { KEY_F(24),        VI_KEY( SHIFT_F12 )     },
    /* Ctrl+function keys: F25-F36 in ncurses */
    { KEY_F(25),        VI_KEY( CTRL_F1 )       },
    { KEY_F(26),        VI_KEY( CTRL_F2 )       },
    { KEY_F(27),        VI_KEY( CTRL_F3 )       },
    { KEY_F(28),        VI_KEY( CTRL_F4 )       },
    { KEY_F(29),        VI_KEY( CTRL_F5 )       },
    { KEY_F(30),        VI_KEY( CTRL_F6 )       },
    { KEY_F(31),        VI_KEY( CTRL_F7 )       },
    { KEY_F(32),        VI_KEY( CTRL_F8 )       },
    { KEY_F(33),        VI_KEY( CTRL_F9 )       },
    { KEY_F(34),        VI_KEY( CTRL_F10 )      },
    { KEY_F(35),        VI_KEY( CTRL_F11 )      },
    { KEY_F(36),        VI_KEY( CTRL_F12 )      },
    /* Shift+arrow keys — some terminals support these */
    { KEY_SLEFT,        VI_KEY( SHIFT_LEFT )    },
    { KEY_SRIGHT,       VI_KEY( SHIFT_RIGHT )   },
    { KEY_SR,           VI_KEY( SHIFT_UP )      },  /* scroll reverse */
    { KEY_SF,           VI_KEY( SHIFT_DOWN )    },  /* scroll forward */
    { KEY_SHOME,        VI_KEY( SHIFT_HOME )    },
    { KEY_SEND,         VI_KEY( SHIFT_END )     },
    { KEY_SPREVIOUS,    VI_KEY( SHIFT_PAGEUP )  },
    { KEY_SNEXT,        VI_KEY( SHIFT_PAGEDOWN )},
    { KEY_SDC,          VI_KEY( SHIFT_DEL )     },
    { KEY_SIC,          VI_KEY( SHIFT_INS )     },
    /* Ctrl+arrow extended codes (xterm, 5xx range) */
    { 560,              VI_KEY( CTRL_LEFT )     },  /* kLFT5 */
    { 561,              VI_KEY( CTRL_RIGHT )    },  /* kRIT5 */
    { 566,              VI_KEY( CTRL_UP )       },  /* kUP5 */
    { 525,              VI_KEY( CTRL_DOWN )     },  /* kDN5 */
    { 536,              VI_KEY( CTRL_HOME )     },  /* kHOM5 */
    { 531,              VI_KEY( CTRL_END )      },  /* kEND5 */
    { 556,              VI_KEY( CTRL_PAGEUP )   },  /* kPRV5 */
    { 551,              VI_KEY( CTRL_PAGEDOWN ) },  /* kNXT5 */
    { 519,              VI_KEY( CTRL_DEL )      },  /* kDC5 */
    { 514,              VI_KEY( CTRL_INS )      },  /* kIC5 */
    /* Mouse events */
    { KEY_MOUSE,        VI_KEY( MOUSEEVENT )    },
    { 0, 0 }  /* sentinel */
};

/*
 * BIOSKeyboardInit - initialize keyboard mapping
 */
int BIOSKeyboardInit( void )
{
    /* ncurses keypad mode already enabled in ScreenInit */
    return( 0 );
}

/*
 * BIOSGetKeyboard - read a key from the terminal.
 *
 * Blocks until a key is available.  Maps ncurses key constants to
 * the editor's vi_key enum.  Ctrl+letter keys (1-26) are mapped to
 * VI_KEY(CTRL_A) through VI_KEY(CTRL_Z).
 */
vi_key BIOSGetKeyboard( int *scan )
{
    int         ch;
    int         i;

    extern void HandleKeyResize( void );

    for( ;; ) {
        ch = getch();

        /* ncurses returns KEY_RESIZE when it detects a terminal
         * resize via its internal SIGWINCH handler.  By this point,
         * ncurses has already called resizeterm() and updated
         * LINES, COLS, and stdscr.  We rebuild the editor windows. */
        if( ch == KEY_RESIZE ) {
            HandleKeyResize();
            continue;
        }

        /* ERR means getch was interrupted — loop and try again */
        if( ch == ERR ) {
            continue;
        }

        break;
    }

    if( scan != NULL ) {
        *scan = 0;
    }

    /* Ctrl+letter: ncurses returns 1-26 for Ctrl-A through Ctrl-Z */
    if( ch >= 1 && ch <= 26 ) {
        return( (vi_key)( VI_KEY( CTRL_A ) + ch - 1 ) );
    }

    /* Printable ASCII: return directly (vi_key values match ASCII) */
    if( ch >= 32 && ch < 127 ) {
        return( (vi_key)ch );
    }

    /* Search the mapping table for special keys */
    for( i = 0; keymaps[i].nckey != 0; i++ ) {
        if( keymaps[i].nckey == ch ) {
            return( keymaps[i].vikey );
        }
    }

    /* Unknown key — ignore */
    return( VI_KEY( DUMMY ) );
}

/*
 * BIOSKeyboardHit - test for pending keyboard input.
 * Uses ncurses nodelay mode to do a non-blocking check.
 */
bool BIOSKeyboardHit( void )
{
    int     ch;

    nodelay( stdscr, TRUE );
    ch = getch();
    nodelay( stdscr, FALSE );
    if( ch != ERR ) {
        ungetch( ch );
        return( TRUE );
    }
    return( FALSE );
}

/*
 * mapDOSAttrToNCurses - convert a DOS-style color attribute byte
 * to an ncurses attribute value.
 *
 * DOS attribute byte layout:
 *   bits 0-2: foreground color (0-7)
 *   bit  3:   bright foreground
 *   bits 4-6: background color (0-7)
 *   bit  7:   blink (we use it as bright background)
 *
 * We map this to an ncurses color pair + bold attribute.
 */
static int mapDOSAttrToNCurses( unsigned char attr )
{
    int     fg, bg, pair;
    int     ncattr;

    fg = attr & 0x07;
    bg = ( attr >> 4 ) & 0x07;

    /* Color pair number: bg * 8 + fg + 1 (pair 0 is default) */
    pair = bg * 8 + fg + 1;
    if( pair > 64 ) pair = 1;
    ncattr = COLOR_PAIR( pair );

    /* Bright foreground (bit 3) → A_BOLD */
    if( attr & 0x08 ) {
        ncattr |= A_BOLD;
    }

    return( ncattr );
}

/*
 * BIOSUpdateScreen - flush a region of the Scrn buffer to the terminal.
 *
 * The editor writes character/attribute pairs into the Scrn[] array
 * (an array of char_info structs).  This function copies the specified
 * region to the ncurses virtual screen and refreshes.
 *
 * offset: byte offset into Scrn (NOT cell offset)
 * length: byte count (NOT cell count)
 */
void BIOSUpdateScreen( unsigned offset, unsigned nbytes )
{
    char_info   *scr;
    int         startCell, endCell, totalCells;
    int         row, col, i;

    if( PageCnt > 0 || EditFlags.Quiet ) {
        return;
    }

    scr = (char_info *)Scrn;
    totalCells = WindMaxWidth * WindMaxHeight;

    /*
     * Match the Windows BIOSUpdateScreen calling convention:
     *   offset = byte offset into Scrn (divide by sizeof(char_info) for cells)
     *   nbytes = added raw to cell offset to find end position
     *
     * UI callers pass nbytes as a cell count (e.g. w->width, 1).
     * The full-screen flush passes W*H*sizeof(char_info) which overshoots
     * but we clamp to the screen size.
     */
    startCell = offset / sizeof( char_info );
    endCell   = startCell + (int)nbytes - 1;

    /* Clamp to screen bounds */
    if( startCell >= totalCells ) {
        return;
    }
    if( endCell >= totalCells ) {
        endCell = totalCells - 1;
    }

    /* Save cursor position so refresh() doesn't move it.
     * BIOSSetCursor is called separately to position the cursor. */
    {
        int     saveRow, saveCol;
        getyx( stdscr, saveRow, saveCol );

        for( i = startCell; i <= endCell; i++ ) {
            int     ncattr;
            char    ch;

            row = i / WindMaxWidth;
            col = i % WindMaxWidth;

            ch = scr[i].ch;
            ncattr = mapDOSAttrToNCurses( (unsigned char)scr[i].attr );

            attrset( ncattr );
            mvaddch( row, col, (unsigned char)ch );
        }
        attrset( A_NORMAL );

        /* Restore cursor so refresh sends it to the right place */
        move( saveRow, saveCol );
        refresh();
    }
}
