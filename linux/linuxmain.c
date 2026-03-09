/****************************************************************************
*
* Description:  Entry point for Linux builds.
*
****************************************************************************/

#include "vi.h"

/* Watcom C runtime globals — set by main(), used by InitializeEditor() */
int     _argc;
char    **_argv;

int main( int argc, char *argv[] )
{
    static char buffer[FILENAME_MAX];

    _argc = argc;
    _argv = argv;
    EXEName = _cmdname( buffer );
    VarAddGlobalStr( "OS", "unix" );
    Comspec = getenv( "SHELL" );
    if( Comspec == NULL ) {
        Comspec = "/bin/sh";
    }
    InitializeEditor();
    EditMain();
    return( 0 );
}
