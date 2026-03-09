/****************************************************************************
*
* Description:  Linux compatibility functions.
*
*               Provides implementations of Watcom C library functions
*               that are not available on Linux/GCC and cannot be
*               implemented as simple macros in clibext.h.
*
****************************************************************************/

#include "vi.h"

/*
 * watcom_setup_env - stub for OW auto-environment setup.
 * Not needed on Linux.
 */
void watcom_setup_env( void )
{
}

/*
 * GetOpt / OptArg / OptInd - Open Watcom-compatible option parser.
 *
 * Works like POSIX getopt() but uses the OW naming conventions expected
 * by init.c.  The '#' in the optstr means "numeric argument" and '-'
 * means "allow options after non-option arguments".
 */
char    *OptArg = NULL;
int     OptInd = 1;

static int  optPos = 1;    /* position within current argv element */

int GetOpt( int *argc, char *argv[], const char *optstr, const char *usagestr[] )
{
    char    *p;
    char    ch;

    (void)usagestr;
    OptArg = NULL;

    while( OptInd < *argc ) {
        if( argv[OptInd] == NULL || argv[OptInd][0] != '-' ||
            argv[OptInd][1] == '\0' ) {
            return( -1 );
        }
        if( argv[OptInd][0] == '-' && argv[OptInd][1] == '-' &&
            argv[OptInd][2] == '\0' ) {
            OptInd++;
            return( -1 );
        }

        ch = argv[OptInd][optPos];
        if( ch == '\0' ) {
            OptInd++;
            optPos = 1;
            continue;
        }

        /* Check if '#' is in optstr — numeric argument */
        if( ch >= '0' && ch <= '9' ) {
            p = strchr( optstr, '#' );
            if( p != NULL ) {
                OptArg = &argv[OptInd][optPos];
                OptInd++;
                optPos = 1;
                return( '#' );
            }
        }

        p = strchr( optstr, ch );
        if( p == NULL ) {
            /* Unknown option */
            optPos++;
            if( argv[OptInd][optPos] == '\0' ) {
                OptInd++;
                optPos = 1;
            }
            return( '?' );
        }

        if( *(p + 1) == ':' ) {
            /* Option requires an argument */
            if( argv[OptInd][optPos + 1] != '\0' ) {
                OptArg = &argv[OptInd][optPos + 1];
            } else {
                OptInd++;
                if( OptInd < *argc ) {
                    OptArg = argv[OptInd];
                } else {
                    optPos = 1;
                    return( '?' );
                }
            }
            OptInd++;
            optPos = 1;
        } else {
            /* Option with no argument */
            optPos++;
            if( argv[OptInd][optPos] == '\0' ) {
                OptInd++;
                optPos = 1;
            }
        }
        return( ch );
    }
    return( -1 );
}
