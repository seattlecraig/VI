/****************************************************************************
*
* Description:  Linux process spawning — shell out to run external commands.
*
*               Suspends ncurses, runs the command via system(), then
*               restores the terminal and redraws the editor.
*
****************************************************************************/

#include "vi.h"
#include <ncurses.h>
#include "vibios.h"

char *ExeExtensions[] = { "" };
int ExeExtensionCount = sizeof( ExeExtensions ) / sizeof( char * );

char *InternalCommands[] = { "" };
int InternalCommandCount = sizeof( InternalCommands ) / sizeof( char * );

/*
 * ResetSpawnScreen - restore the screen after spawning a subprocess
 */
void ResetSpawnScreen( void )
{
    /* Force ncurses to redraw everything */
    clearok( stdscr, TRUE );
    refresh();
}

/*
 * MySpawn - run an external command.
 *
 * Suspends ncurses (restoring the terminal to normal mode), runs
 * the command, waits for the user to press a key, then resumes
 * ncurses and redraws the editor.
 */
int MySpawn( char *cmd )
{
    int     rc;

    /* Suspend ncurses and restore the terminal */
    def_prog_mode();
    endwin();

    rc = system( cmd );

    /* Resume ncurses */
    reset_prog_mode();
    refresh();

    return( rc );
}
