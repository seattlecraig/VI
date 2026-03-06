/****************************************************************************
*
*  getopt.h -- Header for Open Watcom-compatible GetOpt function.
*
*  The real getopt.c/h from Open Watcom lives in the POSIX misc directory.
*  This provides the same interface.
*
****************************************************************************/

#ifndef _GETOPT_H_INCLUDED
#define _GETOPT_H_INCLUDED

/* Global variable that points to the option argument string */
extern char *OptArg;

/* Current option index */
extern int OptInd;

/* Option parsing error character */
extern int OptErr;

/*
 * GetOpt -- Parse command-line options.
 *
 * Works like POSIX getopt() but with Open Watcom naming conventions.
 * Returns the option character, or -1 when done.
 * The 'usagestr' parameter is a pointer to a usage string array (can be NULL).
 */
int GetOpt( int *argc, char *argv[], const char *optstr, const char *usagestr[] );

#endif
