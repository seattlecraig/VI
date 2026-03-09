/****************************************************************************
*
* Description:  Linux mouse handling using ncurses mouse API.
*
*               Provides mouse init/fini/poll functions for the editor's
*               mouse support in terminal emulators that support it.
*
****************************************************************************/

#include "vi.h"
#include <ncurses.h>
#include "mouse.h"

/* Mouse globals — declared extern in globals.h as unsigned short
 * for __LINUX__. Defined here since globals.c excludes them for __UNIX__. */
unsigned short  MouseRow;
unsigned short  MouseCol;
unsigned short  MouseStatus;

static int lastStatus;
static int lastRow;
static int lastCol;

/*
 * SetMouseSpeed - set mouse movement speed (not applicable on terminals)
 */
void SetMouseSpeed( int speed )
{
    (void)speed;
}

/*
 * SetMousePosition - set the mouse position
 */
void SetMousePosition( int row, int col )
{
    lastRow = MouseRow = row;
    lastCol = MouseCol = col;
}

/*
 * PollMouse - poll the mouse for its state
 */
void PollMouse( int *status, int *row, int *col )
{
    *status = MouseStatus;
    *row    = MouseRow;
    *col    = MouseCol;
    MouseStatus = lastStatus;
    MouseRow    = lastRow;
    MouseCol    = lastCol;
    lastStatus  = *status;
    lastRow     = *row;
    lastCol     = *col;
}

/*
 * InitMouse - enable ncurses mouse tracking
 */
void InitMouse( void )
{
    /* Enable reporting of press, release, click, double-click */
    mousemask( ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL );
    /* Short click interval for responsive feel */
    mouseinterval( 150 );
    EditFlags.HasSystemMouse = TRUE;
}

/*
 * FiniMouse - disable mouse tracking
 */
void FiniMouse( void )
{
    mousemask( 0, NULL );
}
