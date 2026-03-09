/****************************************************************************
*
*  regexp.c -- Regular expression engine for Open Watcom VI editor.
*
*  This is a reimplementation of the Henry Spencer regexp library
*  compatible with the Open Watcom VI editor's interface. The original
*  Henry Spencer code is public domain.
*
*  The VI editor's rxwrap.c defines these macros before including the
*  original regexp.c:
*    CASEIGNORE  - bool flag for case-insensitive matching
*    MAGICFLAG   - bool flag for "magic" mode (regex chars are special)
*    MAGICSTR    - string of chars that are magic in no-magic mode
*    REALTABS    - bool flag for real tab handling
*    ALLOC       - memory allocation function (MemAlloc)
*    WANT_EXCLAMATION - enables ! anchor
*
*  Since rxwrap.c #includes this file directly (as a .c file), these
*  macros are available when compiling.
*
*  This implementation provides:
*    RegComp()   - compile a regular expression
*    RegExec()   - execute a compiled regular expression
*    RegExpError - error code (0 = no error)
*    META[]      - metacharacter string
*
*  NOTE: This file is #included by rxwrap.c (not compiled separately).
*        Do NOT add it to the project's source file list.
*
****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * The "internal use only" codes in the compiled regexp program.
 * These are node types in the compiled program.
 */
#define END     0       /* End of program */
#define BOL     1       /* Match beginning of line */
#define EOL     2       /* Match end of line */
#define ANY     3       /* Match any character */
#define ANYOF   4       /* Match any of these characters */
#define ANYBUT  5       /* Match any but these characters */
#define BRANCH  6       /* One of several alternatives */
#define BACK    7       /* Back reference (internal) */
#define EXACTLY 8       /* Match this string exactly */
#define NOTHING 9       /* Match nothing */
#define STAR    10      /* Zero or more of previous */
#define PLUS    11      /* One or more of previous */
#define CARONE  12      /* One of the characters (single char class) */
#define OPEN    20      /* Opening sub-expression (OPEN+1=\1, etc.) */
#define CLOSE   30      /* Closing sub-expression (CLOSE+1=\1, etc.) */

/* Opcodes with operands are >= OPEN */
#define OP(p)       (*(p))
#define NEXT(p)     (((*((p)+1)&0377)<<8) + (*((p)+2)&0377))
#define OPERAND(p)  ((p) + 3)

/* Flags for regnode/regc */
#define HASWIDTH    01  /* Known never to match null string */
#define SIMPLE      02  /* Simple enough to be STAR/PLUS operand */
#define SPSTART     04  /* Starts with * or + */
#define WORST       0   /* Worst case */

/*
 * META characters - regex metacharacters
 * Used externally by rxsupp.c and tags.c
 */
char META[] = ".[{()*+?|^$\\";

/* Global error code */
#ifndef RegExpError
/* RegExpError is declared extern in globals.c, so we don't define it here */
#endif

/* Forward declarations */
static char *reg( int paren, int *flagp );
static char *regbranch( int *flagp );
static char *regpiece( int *flagp );
static char *regatom( int *flagp );
static char *regnode( int op );
static void  regc( int c );
static void  reginsert( int op, char *opnd );
static void  regtail( char *p, char *val );
static void  regoptail( char *p, char *val );
static int   regtry( regexp *prog, char *string );
static int   regmatch( char *prog );
static int   regrepeat( char *p );
static char *regnext( char *p );

/* Compilation state */
static char *regparse;      /* Input scan pointer */
static int   regnpar;       /* Count of () pairs */
static char  regdummy;
static char *regcode;       /* Code emit pointer; &regdummy = don't emit */
static long  regsize;       /* Code size */

/* Execution state */
static char *reginput;      /* String input pointer */
static char *regbol;        /* Beginning of input for ^ matching */
static char **regstartp;    /* Pointer to startp array */
static char **regendp;      /* Pointer to endp array */

/*
 * Case override for @ and ~ prefix characters.
 * 0 = use global CASEIGNORE, 1 = force case-sensitive, 2 = force case-insensitive.
 * Set by RegExec from the compiled regexp's caseover field.
 */
static int reg_caseover;

/* Error codes matching VI's error system */
/* ERR_NO_ERR should be 0, ERR_INVALID_REGEXP should be some error code */
/* We just set RegExpError to 0 for OK or non-zero for error */
#ifndef ERR_NO_ERR
#define ERR_NO_ERR  0
#endif

#define FAIL(m) { RegExpError = 1; return(NULL); }
#define EMSG(m) { RegExpError = 1; }

/*
 * regcaseignore - check if case-insensitive matching is active.
 * Respects per-pattern @ (force sensitive) and ~ (force insensitive)
 * overrides before falling back to the global CASEIGNORE flag.
 */
static int regcaseignore( void )
{
    if( reg_caseover == 1 ) return( 0 );  /* @ = force case-sensitive */
    if( reg_caseover == 2 ) return( 1 );  /* ~ = force case-insensitive */
#ifdef CASEIGNORE
    return( CASEIGNORE );
#else
    return( 0 );
#endif
}

/* Case comparison helpers */
static int regcmp( int c1, int c2 )
{
    if( regcaseignore() ) {
        return( tolower( c1 ) == tolower( c2 ) );
    }
    return( c1 == c2 );
}

/* Check if a character is magic (special in regex) */
static int is_magic( int c )
{
#ifdef MAGICFLAG
    if( !MAGICFLAG ) {
#ifdef MAGICSTR
        /* In no-magic mode, only chars in MAGICSTR are special */
        if( MAGICSTR != NULL && strchr( MAGICSTR, c ) != NULL ) {
            return( 1 );
        }
#endif
        return( 0 );
    }
#endif
    return( strchr( META, c ) != NULL );
}

/*
 * RegComp - compile a regular expression into internal code
 *
 * We allocate memory for the compiled regexp and return it.
 * Returns NULL on error, with RegExpError set.
 */
regexp *RegComp( char *exp )
{
    regexp  *r;
    char    *scan;
    char    *longest;
    int     len;
    int     flags;
    int     caseover = 0;

    RegExpError = ERR_NO_ERR;

    if( exp == NULL ) {
        FAIL( "NULL argument" );
    }

    /*
     * Check for @ (force case-sensitive) or ~ (force case-insensitive)
     * prefix characters. These override the global CASEIGNORE setting
     * for this particular pattern. Used by fgrep when opening matched
     * files to preserve the case-sensitivity of the original search.
     */
    if( exp[0] == '@' ) {
        caseover = 1;   /* force case-sensitive */
        exp++;
    } else if( exp[0] == '~' ) {
        caseover = 2;   /* force case-insensitive */
        exp++;
    }

    /* Set the case override so regcaseignore() returns the right
     * value during compilation (affects character class expansion). */
    reg_caseover = caseover;

    /* First pass: determine size */
    regparse = exp;
    regnpar = 1;
    regsize = 0L;
    regcode = &regdummy;
    regc( (char)0 ); /* magic byte */
    if( reg( 0, &flags ) == NULL ) {
        return( NULL );
    }

    /* Allocate space */
#ifdef ALLOC
    r = (regexp *)ALLOC( sizeof( regexp ) + (unsigned)regsize );
#else
    r = (regexp *)malloc( sizeof( regexp ) + (unsigned)regsize );
#endif
    if( r == NULL ) {
        FAIL( "out of space" );
    }

    /* Second pass: emit code */
    regparse = exp;
    regnpar = 1;
    regcode = r->program;
    regc( (char)0 ); /* magic byte */
    if( reg( 0, &flags ) == NULL ) {
#ifdef ALLOC
        /* ALLOC may use a different free */
#else
        free( r );
#endif
        return( NULL );
    }

    /* Fill in the regexp header */
    r->regstart = '\0';
    r->reganch = 0;
    r->regmust = NULL;
    r->regmlen = 0;
    r->caseover = caseover;

    /* Dig out information for optimizations */
    scan = r->program + 1;     /* First BRANCH */
    if( OP( regnext( scan ) ) == END ) {
        /* Only one alternative */
        scan = OPERAND( scan );

        /* Starting character optimization */
        if( OP( scan ) == EXACTLY ) {
            r->regstart = *OPERAND( scan );
        } else if( OP( scan ) == BOL ) {
            r->reganch++;
        }

        /* Find longest literal string that must appear and save it */
        longest = NULL;
        len = 0;
        for( ; scan != NULL; scan = regnext( scan ) ) {
            if( OP( scan ) == EXACTLY && (int)strlen( OPERAND( scan ) ) >= len ) {
                longest = OPERAND( scan );
                len = (int)strlen( OPERAND( scan ) );
            }
        }
        r->regmust = longest;
        r->regmlen = len;
    }

    return( r );
}

/*
 * reg - the recursive guts of RegComp
 */
static char *reg( int paren, int *flagp )
{
    char    *ret, *br, *ender;
    int     parno = 0;
    int     flags;

    *flagp = HASWIDTH;

    if( paren ) {
        if( regnpar >= NSUBEXP ) {
            FAIL( "too many ()" );
        }
        parno = regnpar;
        regnpar++;
        ret = regnode( OPEN + parno );
    } else {
        ret = NULL;
    }

    /* Pick up the branches, linking them together */
    br = regbranch( &flags );
    if( br == NULL ) return( NULL );
    if( ret != NULL ) {
        regtail( ret, br );
    } else {
        ret = br;
    }
    if( !(flags & HASWIDTH) ) *flagp &= ~HASWIDTH;
    *flagp |= flags & SPSTART;

    while( *regparse == '|' ||
           (!is_magic( '|' ) && *regparse == '\\' && *(regparse+1) == '|') ) {
        if( *regparse == '|' ) {
            regparse++;
        } else {
            regparse += 2;
        }
        br = regbranch( &flags );
        if( br == NULL ) return( NULL );
        regtail( ret, br );
        if( !(flags & HASWIDTH) ) *flagp &= ~HASWIDTH;
        *flagp |= flags & SPSTART;
    }

    /* Make a closing node and hook it in */
    ender = regnode( (paren) ? CLOSE + parno : END );
    regtail( ret, ender );

    /* Hook branch tails to the closing node */
    for( br = ret; br != NULL; br = regnext( br ) ) {
        if( OP( br ) == BRANCH ) {
            regoptail( br, ender );
        }
    }

    /* Check for proper termination */
    if( paren && *regparse++ != ')' ) {
        FAIL( "unmatched ()" );
    }
    if( !paren && *regparse != '\0' ) {
        if( *regparse == ')' ) {
            FAIL( "unmatched ()" );
        }
        FAIL( "junk on end" );
    }

    return( ret );
}

/*
 * regbranch - one alternative of an | operator
 */
static char *regbranch( int *flagp )
{
    char    *ret, *chain, *latest;
    int     flags;

    *flagp = WORST;

    ret = regnode( BRANCH );
    chain = NULL;
    while( *regparse != '\0' && *regparse != '|' && *regparse != ')' ) {
        /* Check for escaped | */
        if( *regparse == '\\' && *(regparse+1) == '|' && !is_magic( '|' ) ) {
            break;
        }
        latest = regpiece( &flags );
        if( latest == NULL ) return( NULL );
        *flagp |= flags & HASWIDTH;
        if( chain == NULL ) {
            *flagp |= flags & SPSTART;
        } else {
            regtail( chain, latest );
        }
        chain = latest;
    }
    if( chain == NULL ) {
        (void)regnode( NOTHING );
    }

    return( ret );
}

/*
 * regpiece - something followed by possible quantifier (* + ?)
 */
static char *regpiece( int *flagp )
{
    char    *ret;
    char    op;
    char    *next;
    int     flags;

    ret = regatom( &flags );
    if( ret == NULL ) return( NULL );

    op = *regparse;
    if( !is_magic( op ) ) {
        *flagp = flags;
        return( ret );
    }

    if( op == '*' || op == '+' || op == '?' ) {
        /* Handle quantifiers */
    } else {
        *flagp = flags;
        return( ret );
    }

    if( !(flags & HASWIDTH) && op != '?' ) {
        FAIL( "*+ operand could be empty" );
    }
    *flagp = (op != '+') ? (WORST | SPSTART) : (WORST | HASWIDTH);

    if( op == '*' && (flags & SIMPLE) ) {
        reginsert( STAR, ret );
    } else if( op == '*' ) {
        reginsert( BRANCH, ret );
        regoptail( ret, regnode( BACK ) );
        regoptail( ret, ret );
        regtail( ret, regnode( BRANCH ) );
        regtail( ret, regnode( NOTHING ) );
    } else if( op == '+' && (flags & SIMPLE) ) {
        reginsert( PLUS, ret );
    } else if( op == '+' ) {
        next = regnode( BRANCH );
        regtail( ret, next );
        regtail( regnode( BACK ), ret );
        regtail( next, regnode( BRANCH ) );
        regtail( ret, regnode( NOTHING ) );
    } else if( op == '?' ) {
        reginsert( BRANCH, ret );
        regtail( ret, regnode( BRANCH ) );
        next = regnode( NOTHING );
        regtail( ret, next );
        regoptail( ret, next );
    }
    regparse++;
    if( *regparse == '*' || *regparse == '+' || *regparse == '?' ) {
        FAIL( "nested *?+" );
    }

    return( ret );
}

/*
 * regatom - the lowest level
 */
static char *regatom( int *flagp )
{
    char    *ret;
    int     flags;
    int     c;

    *flagp = WORST;

    c = *regparse++;
    switch( c ) {
    case '^':
        if( is_magic( '^' ) ) {
            ret = regnode( BOL );
        } else {
            goto defchar;
        }
        break;
    case '$':
        if( is_magic( '$' ) ) {
            ret = regnode( EOL );
        } else {
            goto defchar;
        }
        break;
    case '.':
        if( is_magic( '.' ) ) {
            ret = regnode( ANY );
            *flagp |= HASWIDTH | SIMPLE;
        } else {
            goto defchar;
        }
        break;
    case '[': {
        int     klass;
        int     klassend;

        if( is_magic( '[' ) ) {
            if( *regparse == '^' ) {
                ret = regnode( ANYBUT );
                regparse++;
            } else {
                ret = regnode( ANYOF );
            }
            if( *regparse == ']' || *regparse == '-' ) {
                regc( *regparse++ );
            }
            while( *regparse != '\0' && *regparse != ']' ) {
                if( *regparse == '-' ) {
                    regparse++;
                    if( *regparse == ']' || *regparse == '\0' ) {
                        regc( '-' );
                    } else {
                        klass = (unsigned char)*(regparse - 2) + 1;
                        klassend = (unsigned char)*regparse;
                        if( klass > klassend + 1 ) {
                            FAIL( "invalid [] range" );
                        }
                        for( ; klass <= klassend; klass++ ) {
                            regc( (char)klass );
                            if( regcaseignore() ) {
                                if( isupper( klass ) ) {
                                    regc( (char)tolower( klass ) );
                                } else if( islower( klass ) ) {
                                    regc( (char)toupper( klass ) );
                                }
                            }
                        }
                        regparse++;
                    }
                } else {
                    if( regcaseignore() ) {
                        regc( (char)tolower( *regparse ) );
                        regc( (char)toupper( *regparse ) );
                        regparse++;
                    } else {
                        regc( *regparse++ );
                    }
                }
            }
            regc( '\0' );
            if( *regparse != ']' ) {
                FAIL( "unmatched []" );
            }
            regparse++;
            *flagp |= HASWIDTH | SIMPLE;
        } else {
            goto defchar;
        }
        break;
    }
    case '(':
        if( is_magic( '(' ) ) {
            ret = reg( 1, &flags );
            if( ret == NULL ) return( NULL );
            *flagp |= flags & (HASWIDTH | SPSTART);
        } else {
            goto defchar;
        }
        break;
    case '\0':
    case '|':
    case ')':
        FAIL( "internal urp" );
        /* NOTREACHED */
    case '\\':
        if( *regparse == '\0' ) {
            FAIL( "trailing \\\\" );
        }
        /* Handle \( \) \| as magic if in no-magic mode */
        c = *regparse;
        if( !is_magic( c ) ) {
            if( c == '(' ) {
                regparse++;
                ret = reg( 1, &flags );
                if( ret == NULL ) return( NULL );
                *flagp |= flags & (HASWIDTH | SPSTART);
                break;
            }
            if( c == '|' ) {
                /* handled by higher level */
                regparse--;
                goto defchar;
            }
            if( c == ')' ) {
                regparse++;
                /* will be caught by reg() */
                regparse--;
                regparse--;
                goto defchar;
            }
        }
        /* Escaped character */
        switch( *regparse ) {
        case 't':
            regparse++;
#ifdef REALTABS
            if( REALTABS ) {
                ret = regnode( EXACTLY );
                regc( '\t' );
                regc( '\0' );
            } else {
                ret = regnode( EXACTLY );
                regc( 't' );
                regc( '\0' );
            }
#else
            ret = regnode( EXACTLY );
            regc( '\t' );
            regc( '\0' );
#endif
            *flagp |= HASWIDTH | SIMPLE;
            break;
        case 'n':
            regparse++;
            ret = regnode( EXACTLY );
            regc( '\n' );
            regc( '\0' );
            *flagp |= HASWIDTH | SIMPLE;
            break;
        default:
            ret = regnode( EXACTLY );
            if( regcaseignore() ) {
                regc( (char)tolower( *regparse ) );
            } else {
                regc( *regparse );
            }
            regc( '\0' );
            regparse++;
            *flagp |= HASWIDTH | SIMPLE;
            break;
        }
        break;
#ifdef WANT_EXCLAMATION
    case '!':
        if( is_magic( '!' ) ) {
            ret = regnode( BOL );
        } else {
            goto defchar;
        }
        break;
#endif
    default:
    defchar:
        regparse--;
        {
            int     len = 0;
            char    ender;

            ret = regnode( EXACTLY );
            while( 1 ) {
                c = *regparse;
                if( c == '\0' ) break;
                if( is_magic( c ) ) break;
                if( c == '\\' ) {
                    c = *(regparse + 1);
                    if( c == '\0' ) break;
                    if( !is_magic( c ) && (c == '(' || c == ')' || c == '|') ) break;
                    if( c == 't' || c == 'n' ) break;
                    /* Escaped non-magic char */
                    regparse++;
                }
                if( regcaseignore() ) {
                    regc( (char)tolower( *regparse ) );
                } else {
                    regc( *regparse );
                }
                len++;
                regparse++;
            }
            if( len <= 0 ) {
                FAIL( "internal disaster" );
            }
            ender = *(regparse);
            if( len > 1 && (ender == '*' || ender == '+' || ender == '?') && is_magic( ender ) ) {
                /* Back up to emit the last char separately so quantifier applies to it */
                regcode--;
                regsize--;
                regparse--;
                len--;
            }
            regc( '\0' );
            *flagp |= HASWIDTH;
            if( len == 1 ) *flagp |= SIMPLE;
        }
        break;
    }

    return( ret );
}

/*
 * regnode - emit a node
 */
static char *regnode( int op )
{
    char    *ret;
    char    *ptr;

    ret = regcode;
    if( ret == &regdummy ) {
        regsize += 3;
        return( ret );
    }

    ptr = ret;
    *ptr++ = (char)op;
    *ptr++ = '\0';  /* Null "next" pointer */
    *ptr++ = '\0';
    regcode = ptr;

    return( ret );
}

/*
 * regc - emit a byte to the code
 */
static void regc( int b )
{
    if( regcode != &regdummy ) {
        *regcode++ = (char)b;
    } else {
        regsize++;
    }
}

/*
 * reginsert - insert an operator in front of already-emitted operand
 */
static void reginsert( int op, char *opnd )
{
    char    *src;
    char    *dst;
    char    *place;

    if( regcode == &regdummy ) {
        regsize += 3;
        return;
    }

    src = regcode;
    regcode += 3;
    dst = regcode;
    while( src > opnd ) {
        *--dst = *--src;
    }

    place = opnd;
    *place++ = (char)op;
    *place++ = '\0';
    *place++ = '\0';
}

/*
 * regtail - set the next-pointer at the end of a node chain
 */
static void regtail( char *p, char *val )
{
    char    *scan;
    char    *temp;
    int     offset;

    if( p == &regdummy ) return;

    /* Find last node */
    scan = p;
    for( ;; ) {
        temp = regnext( scan );
        if( temp == NULL ) break;
        scan = temp;
    }

    if( OP( scan ) == BACK ) {
        offset = (int)(scan - val);
    } else {
        offset = (int)(val - scan);
    }
    *(scan + 1) = (char)((offset >> 8) & 0377);
    *(scan + 2) = (char)(offset & 0377);
}

/*
 * regoptail - regtail on operand of first argument; nop if operandless
 */
static void regoptail( char *p, char *val )
{
    if( p == NULL || p == &regdummy || OP( p ) != BRANCH ) return;
    regtail( OPERAND( p ), val );
}

/*
 * RegExec - match a regexp against a string
 */
int RegExec( regexp *prog, char *string, int bol )
{
    char    *s;

    RegExpError = ERR_NO_ERR;

    if( prog == NULL || string == NULL ) {
        RegExpError = 1;
        return( 0 );
    }

    /* Check validity of program */
    if( *(prog->program) != 0 ) {
        RegExpError = 1;
        return( 0 );
    }

    /* Set per-pattern case override from @ or ~ prefix (stored at compile time) */
    reg_caseover = prog->caseover;

    /* If there is a "must appear" string, look for it.
     * When case-insensitive, skip this optimization because strchr
     * is case-sensitive and would miss matches where the first
     * character differs in case (e.g., searching for "dir" won't
     * find "Dir" because strchr looks for lowercase 'd' only). */
    if( prog->regmust != NULL && !regcaseignore() ) {
        s = string;
        while( (s = strchr( s, prog->regmust[0] )) != NULL ) {
            if( strncmp( s, prog->regmust, prog->regmlen ) == 0 ) break;
            s++;
        }
        if( s == NULL ) return( 0 );
    }

    /* Set bol marker */
    regbol = bol ? string : NULL;

    /* Simplest case: anchored match */
    if( prog->reganch ) {
        return( regtry( prog, string ) );
    }

    /* Messy cases: unanchored match */
    s = string;
    if( prog->regstart != '\0' ) {
        /* We know what char it must start with */
        while( s != NULL ) {
            if( regcaseignore() ) {
                while( *s != '\0' && tolower( *s ) != tolower( prog->regstart ) ) {
                    s++;
                }
            } else {
                while( *s != '\0' && *s != prog->regstart ) {
                    s++;
                }
            }
            if( *s == '\0' ) break;
            if( regtry( prog, s ) ) return( 1 );
            s++;
        }
    } else {
        /* We don't, so try every position */
        do {
            if( regtry( prog, s ) ) return( 1 );
        } while( *s++ != '\0' );
    }

    return( 0 );
}

/*
 * regtry - try match at specific point
 */
static int regtry( regexp *prog, char *string )
{
    int     i;
    char    **sp;
    char    **ep;

    reginput = string;
    regstartp = prog->startp;
    regendp = prog->endp;

    sp = prog->startp;
    ep = prog->endp;
    for( i = NSUBEXP; i > 0; i-- ) {
        *sp++ = NULL;
        *ep++ = NULL;
    }

    if( regmatch( prog->program + 1 ) ) {
        prog->startp[0] = string;
        prog->endp[0] = reginput;
        return( 1 );
    }
    return( 0 );
}

/*
 * regmatch - main matching routine
 *
 * Conceptually the strategy is simple: check to see whether the current
 * node matches, call self recursively to see whether the rest matches,
 * and then act accordingly.
 */
static int regmatch( char *prog )
{
    char    *scan;
    char    *next;

    scan = prog;
    while( scan != NULL ) {
        next = regnext( scan );

        switch( OP( scan ) ) {
        case BOL:
            if( reginput != regbol ) return( 0 );
            break;
        case EOL:
            if( *reginput != '\0' ) return( 0 );
            break;
        case ANY:
            if( *reginput == '\0' ) return( 0 );
            reginput++;
            break;
        case EXACTLY: {
            char    *opnd = OPERAND( scan );
            int     len = (int)strlen( opnd );

            if( regcaseignore() ) {
                if( _strnicmp( reginput, opnd, len ) != 0 ) return( 0 );
            } else {
                if( strncmp( reginput, opnd, len ) != 0 ) return( 0 );
            }
            reginput += len;
            break;
        }
        case ANYOF:
            if( *reginput == '\0' ) return( 0 );
            if( regcaseignore() ) {
                if( strchr( OPERAND( scan ), tolower( *reginput ) ) == NULL &&
                    strchr( OPERAND( scan ), toupper( *reginput ) ) == NULL )
                    return( 0 );
            } else {
                if( strchr( OPERAND( scan ), *reginput ) == NULL ) return( 0 );
            }
            reginput++;
            break;
        case ANYBUT:
            if( *reginput == '\0' ) return( 0 );
            if( regcaseignore() ) {
                if( strchr( OPERAND( scan ), tolower( *reginput ) ) != NULL ||
                    strchr( OPERAND( scan ), toupper( *reginput ) ) != NULL )
                    return( 0 );
            } else {
                if( strchr( OPERAND( scan ), *reginput ) != NULL ) return( 0 );
            }
            reginput++;
            break;
        case NOTHING:
            break;
        case BACK:
            break;
        case BRANCH: {
            char    *save;

            if( OP( next ) != BRANCH ) {
                /* No choice */
                next = OPERAND( scan );
            } else {
                do {
                    save = reginput;
                    if( regmatch( OPERAND( scan ) ) ) return( 1 );
                    reginput = save;
                    scan = regnext( scan );
                } while( scan != NULL && OP( scan ) == BRANCH );
                return( 0 );
            }
            break;
        }
        case STAR:
        case PLUS: {
            char    nextch;
            int     no;
            char    *save;
            int     min;

            nextch = (OP( next ) == EXACTLY) ? *OPERAND( next ) : '\0';
            min = (OP( scan ) == STAR) ? 0 : 1;
            save = reginput;
            no = regrepeat( OPERAND( scan ) );
            while( no >= min ) {
                /* If it could work, try it */
                if( nextch == '\0' || regcmp( *reginput, nextch ) ) {
                    if( regmatch( next ) ) return( 1 );
                }
                /* Couldn't or didn't, back up */
                no--;
                reginput = save + no;
            }
            return( 0 );
        }
        case END:
            return( 1 );
        default:
            if( OP( scan ) >= OPEN && OP( scan ) < OPEN + NSUBEXP ) {
                int     no = OP( scan ) - OPEN;
                char    *save = reginput;

                if( regmatch( next ) ) {
                    regstartp[no] = save;
                    return( 1 );
                }
                return( 0 );
            } else if( OP( scan ) >= CLOSE && OP( scan ) < CLOSE + NSUBEXP ) {
                int     no = OP( scan ) - CLOSE;
                char    *save = reginput;

                if( regmatch( next ) ) {
                    regendp[no] = save;
                    return( 1 );
                }
                return( 0 );
            }
            RegExpError = 1;
            return( 0 );
        }

        scan = next;
    }

    /* We get here only if there's trouble */
    RegExpError = 1;
    return( 0 );
}

/*
 * regrepeat - repeatedly match something simple, report how many
 */
static int regrepeat( char *p )
{
    int     count = 0;
    char    *scan;
    char    *opnd;

    scan = reginput;
    opnd = OPERAND( p );
    switch( OP( p ) ) {
    case ANY:
        count = (int)strlen( scan );
        scan += count;
        break;
    case EXACTLY:
        if( regcaseignore() ) {
            while( tolower( *scan ) == tolower( *opnd ) ) {
                count++;
                scan++;
            }
        } else {
            while( *scan == *opnd ) {
                count++;
                scan++;
            }
        }
        break;
    case ANYOF:
        if( regcaseignore() ) {
            while( *scan != '\0' &&
                   (strchr( opnd, tolower( *scan ) ) != NULL ||
                    strchr( opnd, toupper( *scan ) ) != NULL) ) {
                count++;
                scan++;
            }
        } else {
            while( *scan != '\0' && strchr( opnd, *scan ) != NULL ) {
                count++;
                scan++;
            }
        }
        break;
    case ANYBUT:
        if( regcaseignore() ) {
            while( *scan != '\0' &&
                   strchr( opnd, tolower( *scan ) ) == NULL &&
                   strchr( opnd, toupper( *scan ) ) == NULL ) {
                count++;
                scan++;
            }
        } else {
            while( *scan != '\0' && strchr( opnd, *scan ) == NULL ) {
                count++;
                scan++;
            }
        }
        break;
    default:
        /* Something bad */
        RegExpError = 1;
        count = 0;
        break;
    }
    reginput = scan;

    return( count );
}

/*
 * regnext - dig the "next" pointer out of a node
 */
static char *regnext( char *p )
{
    int     offset;

    if( p == &regdummy ) return( NULL );

    offset = NEXT( p );
    if( offset == 0 ) return( NULL );

    if( OP( p ) == BACK ) {
        return( p - offset );
    } else {
        return( p + offset );
    }
}
