/****************************************************************************
*
*  getopt.c -- Open Watcom-compatible GetOpt implementation for MSVC.
*
*  This reimplements the GetOpt() function from Open Watcom's POSIX library.
*  It parses command-line options in the standard Unix getopt style, but
*  uses the Open Watcom naming conventions (GetOpt, OptArg, etc.).
*
****************************************************************************/

#include <stdio.h>
#include <string.h>

/* Global variables matching Open Watcom getopt interface */
char *OptArg = NULL;    /* Points to the argument for the current option */
int   OptInd = 1;       /* Index of the next argv element to process */
int   OptErr = 1;       /* If non-zero, print error messages */

/* Internal state */
static int  optPos = 1; /* Position within the current argv element */

/*
 * GetOpt -- Parse command-line options.
 *
 * Processes argv[] looking for options specified in optstr.
 * A colon after an option character in optstr means that option takes an argument.
 * Returns the option character found, or -1 when all options are parsed.
 *
 * Note: The Open Watcom version takes argc as int* (it can modify it).
 * The usagestr parameter is ignored in this implementation.
 */
int GetOpt( int *argc, char *argv[], const char *optstr, const char *usagestr[] )
{
    char        *cp;
    char        c;

    (void)usagestr; /* unused in this implementation */

    OptArg = NULL;

    /* Skip past non-option arguments or end of args */
    if( OptInd >= *argc ) {
        return( -1 );
    }
    if( argv[OptInd] == NULL ) {
        return( -1 );
    }

    /* Check if current arg starts with '-' */
    if( argv[OptInd][0] != '-' ) {
        return( -1 );
    }

    /* "--" means end of options */
    if( argv[OptInd][1] == '-' && argv[OptInd][2] == '\0' ) {
        OptInd++;
        return( -1 );
    }

    /* Empty "-" is not an option */
    if( argv[OptInd][1] == '\0' ) {
        return( -1 );
    }

    /* Get the option character */
    c = argv[OptInd][optPos];
    if( c == '\0' ) {
        /* Moved past end of this arg, go to next */
        OptInd++;
        optPos = 1;
        if( OptInd >= *argc || argv[OptInd][0] != '-' || argv[OptInd][1] == '\0' ) {
            return( -1 );
        }
        if( argv[OptInd][1] == '-' && argv[OptInd][2] == '\0' ) {
            OptInd++;
            return( -1 );
        }
        c = argv[OptInd][optPos];
    }

    /* Look up the option character in optstr */
    cp = strchr( optstr, c );
    if( cp == NULL || c == ':' ) {
        /* Unknown option */
        if( OptErr ) {
            fprintf( stderr, "Unknown option: -%c\n", c );
        }
        optPos++;
        if( argv[OptInd][optPos] == '\0' ) {
            OptInd++;
            optPos = 1;
        }
        return( '?' );
    }

    /* Check if this option takes an argument */
    if( cp[1] == ':' ) {
        /* Option takes an argument */
        if( argv[OptInd][optPos + 1] != '\0' ) {
            /* Argument is rest of current argv element */
            OptArg = &argv[OptInd][optPos + 1];
        } else {
            /* Argument is the next argv element */
            OptInd++;
            if( OptInd >= *argc ) {
                if( OptErr ) {
                    fprintf( stderr, "Option -%c requires an argument\n", c );
                }
                optPos = 1;
                return( '?' );
            }
            OptArg = argv[OptInd];
        }
        OptInd++;
        optPos = 1;
    } else {
        /* No argument */
        optPos++;
        if( argv[OptInd][optPos] == '\0' ) {
            OptInd++;
            optPos = 1;
        }
    }

    return( c );
}
