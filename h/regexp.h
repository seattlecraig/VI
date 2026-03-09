/****************************************************************************
*
*  regexp.h -- Regular expression header (Henry Spencer regexp).
*
*  This matches the interface expected by the Open Watcom VI editor.
*  Based on the public domain Henry Spencer regexp implementation
*  as modified by Open Watcom.
*
*  The regexp struct and function prototypes must match what regexp.c
*  (included via rxwrap.c) provides.
*
****************************************************************************/

#ifndef _REGEXP_H_INCLUDED
#define _REGEXP_H_INCLUDED

/* Number of sub-expressions supported (0 = whole match, 1-9 = \1-\9) */
#define NSUBEXP  10

/* Compiled regular expression structure */
typedef struct regexp {
    char    *startp[NSUBEXP];   /* Start pointers for sub-expressions */
    char    *endp[NSUBEXP];     /* End pointers for sub-expressions */
    char    regstart;           /* Internal use: first char if known, '\0' otherwise */
    char    reganch;            /* Internal use: is the match anchored (^ at start)? */
    char    *regmust;           /* Internal use: string that match must include, or NULL */
    int     regmlen;            /* Internal use: length of regmust string */
    int     caseover;           /* Case override: 0=use global, 1=force sensitive, 2=force insensitive */
    char    program[1];         /* Internal use: compiled program (variable length) */
} regexp;

/* Compile a regular expression pattern into a regexp structure.
 * Returns a malloc'd regexp, or NULL on error (sets RegExpError).
 * The caller must free the returned regexp when done. */
extern regexp   *RegComp( char *exp );

/* Execute a compiled regular expression against a string.
 * Returns 1 if match found, 0 if not.
 * If matched, startp[0]/endp[0] point to the matched region,
 * and startp[1..9]/endp[1..9] to sub-expression matches. */
extern int      RegExec( regexp *prog, char *string, int bol );

/*
 * Error code set by RegComp/RegExec.
 * 0 (ERR_NO_ERR) means no error.
 */
extern int      RegExpError;

/*
 * META characters string - regex metacharacters that need escaping.
 * Defined by regexp.c, used by rxsupp.c and tags.c.
 */
extern char     META[];

#endif
