/****************************************************************************
*
*                            Open Watcom Project
*
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  OS/2 specific system interface functions.:
*
****************************************************************************/


#include "vi.h"
#include <dos.h>
#include "win.h"
#include "dosx.h"
#include "vibios.h"
#include "dc.h"

HANDLE  InputHandle, OutputHandle;
static HANDLE  OrigOutputHandle;    /* original screen buffer, restored on exit */
COORD   BSize;

extern int PageCnt;

static char oldDir[VI_MAX_PATH];

/*
 * PushDirectory
 */
void PushDirectory( char *orig )
{
    orig = orig;
    oldDir[0] = 0;
    GetCWD2( oldDir, VI_MAX_PATH );

} /* PushDirectory */

/*
 * PopDirectory
 */
void PopDirectory( void )
{
    if( oldDir[0] != 0 ) {
        ChangeDirectory( oldDir );
    }
    ChangeDirectory( CurrentDirectory );

} /* PopDirectory */

/*
 * NewCursor - change cursor to insert mode type
 */
void NewCursor( window_id id, cursor_type ct )
{
    CONSOLE_CURSOR_INFO ci;

    id = id;
    ci.dwSize = ct.height;
    ci.dwSize += 18;
    if( ci.dwSize > 100 ) {
        ci.dwSize = 100;
    }
    ci.bVisible = TRUE;
    SetConsoleCursorInfo( OutputHandle, &ci );

} /* NewCursor */

/*
 * MyBeep - ring beeper
 */
void MyBeep( void )
{
    if( EditFlags.BeepFlag ) {
        Beep( 300, 75 );
    }

} /* MyBeep */

static char *oldConTitle;

/*
 * ScreenInit - get screen info
 *
 * Uses the console's own screen buffer directly (no CreateConsoleScreenBuffer)
 * so the editor always matches the actual console window size. Enables
 * ENABLE_WINDOW_INPUT so we receive resize events.
 */
void ScreenInit( void )
{
    DWORD                       size;
    COORD                       bufSize;
    char                        tmp[256];

    /*
     * Force console output code page to 437 (OEM/DOS). WriteConsoleOutput
     * uses AsciiChar interpreted via this code page. If a previous program
     * enabled VT mode, the code page may have been changed to 65001 (UTF-8),
     * which causes CP437 box-drawing bytes (0xDA, 0xB3, etc.) to render
     * as diamond-question-mark replacement characters.
     */
    SetConsoleOutputCP( 437 );

    /*
     * Open CONIN$ and CONOUT$ explicitly. These always refer to the
     * actual console, even when stdout/stdin are redirected (ConPTY,
     * VS debugger, piped output, etc).
     */
    InputHandle = CreateFile( "CONIN$", GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, 0, NULL );
    /* ENABLE_EXTENDED_FLAGS without ENABLE_QUICK_EDIT_MODE disables the
     * console's built-in mouse handling (text selection, wheel scrolling
     * the buffer). All mouse events go to the application instead. */
    SetConsoleMode( InputHandle, ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT
                                 | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS );

    /*
     * Save the original console screen buffer so we can restore it
     * on exit. This preserves the shell's scrollback history and
     * prior output — the editor paints on its own private buffer.
     */
    OrigOutputHandle = CreateFile( "CONOUT$", GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                    OPEN_EXISTING, 0, NULL );

    /*
     * Get the console window dimensions from the original buffer
     * BEFORE creating our private buffer.
     */
    {
        CONSOLE_SCREEN_BUFFER_INFO  sbi;

        memset( &sbi, 0, sizeof( sbi ) );
        GetConsoleScreenBufferInfo( OrigOutputHandle, &sbi );

        WindMaxWidth  = sbi.srWindow.Right  - sbi.srWindow.Left + 1;
        WindMaxHeight = sbi.srWindow.Bottom - sbi.srWindow.Top  + 1;

        /* Sanity check for broken ConPTY (VS debugger) */
        if( WindMaxWidth > 1000 || WindMaxHeight > 1000 ) {
            WindMaxWidth  = 120;
            WindMaxHeight = 30;
        }
    }

    /*
     * Create a private screen buffer for the editor. This is the
     * "alternate screen" — when we exit, we switch back to
     * OrigOutputHandle and the user's previous console content
     * reappears untouched.
     */
    OutputHandle = CreateConsoleScreenBuffer( GENERIC_READ | GENERIC_WRITE,
                                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                                              NULL, CONSOLE_TEXTMODE_BUFFER, NULL );

    BSize.X = WindMaxWidth;
    BSize.Y = WindMaxHeight;

    /* Set our buffer to exactly match the window — no scrollbars */
    bufSize.X = WindMaxWidth;
    bufSize.Y = WindMaxHeight;
    SetConsoleScreenBufferSize( OutputHandle, bufSize );

    /* Make our buffer the active (visible) one */
    SetConsoleActiveScreenBuffer( OutputHandle );

    EditFlags.Color = TRUE;

    size = WindMaxWidth * WindMaxHeight * sizeof( char_info );
    Scrn = malloc( size );
    ScreenPage( 0 );

    tmp[0] = 0;
    GetConsoleTitle( tmp, sizeof( tmp ) );
    AddString( &oldConTitle, tmp );
    if( !EditFlags.Quiet ) {
        SetConsoleTitle( "Craig's VI" );
    }

} /* ScreenInit */

/*
 * HandleConsoleResize - called when the console window size changes.
 *
 * Reallocates the screen buffer, updates global size variables, and
 * triggers a full redraw of all editor windows at the new size.
 */
void HandleConsoleResize( int newW, int newH )
{
    COORD                       bufSize;
    DWORD                       safeSize;
    int                         maxW, maxH;
    int                         i;
    static bool                 inResize = FALSE;

    /* Guard against re-entrancy: SetConsoleScreenBufferSize generates
     * another WINDOW_BUFFER_SIZE_EVENT which would call us again. */
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
     * could exceed the new screen size. The oversized buffer prevents
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
    BSize.X = WindMaxWidth;
    BSize.Y = WindMaxHeight;

    /* Set console screen buffer to match the new window size so
     * WriteConsoleOutput covers the full visible area. */
    bufSize.X = WindMaxWidth;
    bufSize.Y = WindMaxHeight;
    SetConsoleScreenBufferSize( OutputHandle, bufSize );

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

        /* DCDisplayAllLines (called by ReDisplayScreen) only marks
         * lines dirty. DCUpdate actually renders them to the Scrn
         * buffer. Normally called from the edit loop, but we need
         * it here so the resize is visible immediately. */
        DCUpdate();

        /* Force a full-screen flush to the console. */
        MyVioShowBuf( 0, WindMaxWidth * WindMaxHeight * sizeof( char_info ) );
    }

    inResize = FALSE;

} /* HandleConsoleResize */

/*
 * ScreenFini - finished with console
 */
void ScreenFini( void )
{
    /* Switch back to the original console screen buffer.
     * This restores the shell's previous output and scrollback
     * history as if the editor was never there. */
    SetConsoleActiveScreenBuffer( OrigOutputHandle );

    CloseHandle( OutputHandle );
    CloseHandle( OrigOutputHandle );
    CloseHandle( InputHandle );
    SetConsoleTitle( oldConTitle );

} /* ScreenFini */

/*
 * ChkExtendedKbd - look for extended keyboard type
 */
void ChkExtendedKbd( void )
{
    EditFlags.ExtendedKeyboard = 0x10;

} /* ChkExtendedKbd */

/*
 * MemSize - return amount of dos memory left (in 16 byte paragraphs)
 */
long MemSize( void )
{
    // this value is not used for anything important.
    return( 0 );

} /* MemSize */

/*
 * ScreenPage - set the screen page to active/inactive
 */
void ScreenPage( int page )
{
    PageCnt += page;

} /* ScreenPage */

/*
 * ChangeDrive - change the working drive
 */
vi_rc ChangeDrive( int drive )
{
    char        dir[4];

    dir[0] = drive;
    dir[1] = ':';
    dir[2] = '.';
    dir[3] = 0;

    if( !SetCurrentDirectory( dir ) ) {
        return( ERR_NO_SUCH_DRIVE );
    }
    return( ERR_NO_ERR );

}/* ChangeDrive */

/*
 * ShiftDown - test if shift key is down
 */
bool ShiftDown( void )
{
    // This is technically correct but this function is not
    // actually used for anything so why bother.

    // BYTE kbstate[256];
    // GetKeyboardState( &kbstate );
    // return( kbstate[VK_SHIFT] & 0x80 );

    return( FALSE );

} /* ShiftDown */

static bool hadCapsLock;

/*
 * TurnOffCapsLock - switch off caps lock
 */
void TurnOffCapsLock( void )
{
    hadCapsLock = FALSE;

} /* TurnOffCapsLock */

/*
 * DoGetDriveType - get the type of drive A-Z
 */
drive_type DoGetDriveType( int drv )
{
    char        path[4];
    DWORD       type;

    path[0] = drv;
    path[1] = ':';
    path[2] = '\\';
    path[3] = 0;
    type = GetDriveType( path );
    if( type == 1 ) {
        return( DRIVE_NONE );
    }
    if( type == DRIVE_REMOVABLE ) {
        return( DRIVE_IS_REMOVABLE );
    }
    return( DRIVE_IS_FIXED );

} /* DoGetDriveType */

/*
 * MyDelay - delay a specified number of milliseconds
 */
void MyDelay( int ms )
{
    Sleep( ms );

} /* MyDelay */

/*
 * SetCursorBlinkRate - set the current blink rate for the cursor
 */
void SetCursorBlinkRate( int cbr )
{
    CursorBlinkRate = cbr;

} /* SetCursorBlinkRate */

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

