/****************************************************************************
*
* Description:  Linux system support functions — ncurses-based screen,
*               cursor, beep, delays, drives (stubs), etc.
*
*               Implements the same interface as nt/ntsys.c but using
*               ncurses instead of Win32 Console API.
*
****************************************************************************/

#include "vi.h"
#include <ncurses.h>
#include "win.h"
#include "dc.h"
#include "vibios.h"

extern int PageCnt;

static void HandleConsoleResize( int newW, int newH );

static char oldPath[FILENAME_MAX];

/*
 * PushDirectory - save current directory
 */
void PushDirectory( char *orig )
{
    (void)orig;
    oldPath[0] = 0;
    GetCWD2( oldPath, FILENAME_MAX );
}

/*
 * PopDirectory - restore saved directory
 */
void PopDirectory( void )
{
    if( oldPath[0] != 0 ) {
        ChangeDirectory( oldPath );
    }
    ChangeDirectory( CurrentDirectory );
}

/*
 * NewCursor - change cursor to insert mode type.
 * Uses ncurses curs_set: 0=invisible, 1=normal, 2=very visible.
 */
void NewCursor( window_id id, cursor_type ct )
{
    (void)id;
    if( ct.height > 50 ) {
        curs_set( 2 );  /* block/very visible */
    } else {
        curs_set( 1 );  /* underline/normal */
    }
}

/*
 * MyBeep - ring terminal bell
 */
void MyBeep( void )
{
    if( EditFlags.BeepFlag ) {
        beep();
    }
}

/*
 * HandleKeyResize - called from the keyboard read path when
 * getch() returns KEY_RESIZE.
 *
 * ncurses handles SIGWINCH internally (if we don't install our
 * own handler).  On receipt of SIGWINCH, ncurses calls resizeterm()
 * to update its data structures and queues KEY_RESIZE for getch().
 * By the time we get here, LINES/COLS and stdscr are already the
 * new size.  We just need to rebuild the editor's windows.
 */
void HandleKeyResize( void )
{
    int     newW, newH;

    getmaxyx( stdscr, newH, newW );
    if( newW != WindMaxWidth || newH != WindMaxHeight ) {
        HandleConsoleResize( newW, newH );
    }
}

/*
 * HandleConsoleResize - called when the terminal size changes.
 *
 * Reallocates the screen buffer, updates global size variables,
 * and triggers a full redraw of all editor windows at the new size.
 */
static void HandleConsoleResize( int newW, int newH )
{
    size_t  safeSize;
    int     maxW, maxH;
    int     i;
    static bool inResize = FALSE;

    /* Guard against re-entrancy */
    if( inResize ) {
        return;
    }

    /* Sanity check */
    if( newW <= 0 || newH <= 0 || newW > 500 || newH > 300 ) {
        return;
    }

    /* Nothing to do if size hasn't actually changed */
    if( newW == WindMaxWidth && newH == WindMaxHeight ) {
        return;
    }

    inResize = TRUE;

    /*
     * Allocate buffers large enough for BOTH old and new dimensions.
     * During the transition, old windows have stale coordinates that
     * could exceed the new screen size.  The oversized buffer prevents
     * out-of-bounds writes when those windows are closed/recreated.
     */
    maxW = ( newW > WindMaxWidth )  ? newW : WindMaxWidth;
    maxH = ( newH > WindMaxHeight ) ? newH : WindMaxHeight;
    safeSize = maxW * maxH * sizeof( char_info );

    free( Scrn );
    Scrn = malloc( safeSize );
    memset( Scrn, 0, safeSize );

    MemFree( ScreenImage );
    ScreenImage = MemAlloc( maxW * maxH );
    for( i = 0; i < maxW * maxH; i++ ) {
        ScreenImage[i] = NO_CHAR;
    }

    /* Now update global screen dimensions */
    WindMaxWidth  = newW;
    WindMaxHeight = newH;

    /* Recalculate all window positions based on new screen size */
    SetWindowSizes();

    /* Resize and redraw all windows */
    if( EditFlags.WindowsStarted ) {
        ResetAllWindows();
        NewMessageWindow();
        if( EditFlags.StatusInfo ) {
            NewStatusWindow();
        }
        ReDisplayScreen();

        /* DCUpdate renders dirty lines to the Scrn buffer */
        DCUpdate();

        /* Tell ncurses the entire screen is stale so every cell
         * gets repainted on the next refresh */
        clearok( stdscr, TRUE );

        /* Force a full-screen flush */
        MyVioShowBuf( 0, WindMaxWidth * WindMaxHeight * sizeof( char_info ) );

        /* Re-establish the cursor position in the current window */
        SetWindowCursor();
        SetWindowCursorForReal();
    }

    inResize = FALSE;
}

/*
 * ScreenInit - initialize ncurses and set up the screen buffer.
 *
 * Allocates the Scrn backing buffer that the editor's display code
 * writes into.  BIOSUpdateScreen then copies regions of Scrn to the
 * ncurses virtual screen.
 */
void ScreenInit( void )
{
    size_t  size;

    initscr();
    raw();                  /* pass all keys through */
    noecho();               /* don't echo typed characters */
    nonl();                 /* don't translate CR to NL */
    scrollok( stdscr, FALSE ); /* don't scroll when writing to bottom-right */
    keypad( stdscr, TRUE ); /* enable function keys, arrow keys */
    nodelay( stdscr, FALSE );
    start_color();
    use_default_colors();

    /* Initialize 64 color pairs covering all 16x16 fg/bg combinations.
     * We map the DOS/OW 4-bit color scheme to curses COLOR_ constants. */
    {
        static short dos2curses[16] = {
            COLOR_BLACK, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN,
            COLOR_RED, COLOR_MAGENTA, COLOR_YELLOW, COLOR_WHITE,
            COLOR_BLACK, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN,
            COLOR_RED, COLOR_MAGENTA, COLOR_YELLOW, COLOR_WHITE
        };
        int fg, bg, pair;
        pair = 1;
        for( bg = 0; bg < 8; bg++ ) {
            for( fg = 0; fg < 8; fg++ ) {
                init_pair( pair, dos2curses[fg], dos2curses[bg] );
                pair++;
            }
        }
    }

    BIOSKeyboardInit();

    WindMaxHeight = LINES;
    WindMaxWidth  = COLS;

    EditFlags.Color = TRUE;

    size = WindMaxWidth * WindMaxHeight * sizeof( char_info );
    Scrn = malloc( size );
    memset( Scrn, 0, size );
    ScreenPage( 0 );

    /* Terminal resize: ncurses installs its own SIGWINCH handler
     * during initscr().  Do NOT install a custom one — that would
     * prevent ncurses from detecting the resize and generating
     * KEY_RESIZE from getch(). */

    if( !EditFlags.Quiet ) {
        /* No console title on Linux terminals in general,
         * but we could set the xterm title if desired. */
    }
}

/*
 * ScreenFini - shut down ncurses and restore terminal
 */
void ScreenFini( void )
{
    endwin();
}

/*
 * ChkExtendedKbd - look for extended keyboard type
 */
void ChkExtendedKbd( void )
{
    EditFlags.ExtendedKeyboard = 0;
}

/*
 * MemSize - return amount of dos memory left (in 16 byte paragraphs).
 * Not meaningful on Linux.
 */
long MemSize( void )
{
    return( 0 );
}

/*
 * ScreenPage - set the screen page to active/inactive
 */
void ScreenPage( int page )
{
    PageCnt += page;
}

/*
 * ChangeDrive - change the working drive.
 * No drive letters on Linux.
 */
vi_rc ChangeDrive( int drive )
{
    (void)drive;
    return( ERR_NO_ERR );
}

/*
 * ShiftDown - test if shift key is down.
 * Not detectable from a terminal.
 */
bool ShiftDown( void )
{
    return( FALSE );
}

static bool hadCapsLock;

/*
 * TurnOffCapsLock - switch off caps lock.
 * Not controllable from a terminal.
 */
void TurnOffCapsLock( void )
{
    hadCapsLock = FALSE;
}

/*
 * DoGetDriveType - get the type of drive A-Z.
 * No drives on Linux.
 */
drive_type DoGetDriveType( int drv )
{
    (void)drv;
    return( DRIVE_NONE );
}

/*
 * MyDelay - delay a specified number of milliseconds
 */
void MyDelay( int ms )
{
    napms( ms );
}

/*
 * SetCursorBlinkRate - set the current blink rate for the cursor
 */
void SetCursorBlinkRate( int cbr )
{
    CursorBlinkRate = cbr;
}

vi_key GetKeyboard( void )
{
    return( GetVIKey( BIOSGetKeyboard( NULL ), 0, FALSE ) );
}

bool KeyboardHit( void )
{
    return( BIOSKeyboardHit() );
}

void MyVioShowBuf( unsigned offset, unsigned length )
{
    BIOSUpdateScreen( offset, length );
}
