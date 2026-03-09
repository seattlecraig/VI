/****************************************************************************
*
*  clibext.h -- GCC/Linux compatibility shim for Watcom C library extensions.
*
*  Provides POSIX-like functions and Watcom-specific functions that the
*  editor's core code expects but that differ or are missing on Linux/GCC.
*
****************************************************************************/

#ifndef _CLIBEXT_H_INCLUDED
#define _CLIBEXT_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>    /* strcasecmp, strncasecmp */
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

/* POSIX access() mode constants */
#ifndef F_OK
  #define F_OK  0
#endif
#ifndef W_OK
  #define W_OK  2
#endif
#ifndef R_OK
  #define R_OK  4
#endif

/* DOS file attribute constants (used by the editor's directory code) */
#ifndef _A_NORMAL
  #define _A_NORMAL   0x00
#endif
#ifndef _A_RDONLY
  #define _A_RDONLY   0x01
#endif
#ifndef _A_HIDDEN
  #define _A_HIDDEN   0x02
#endif
#ifndef _A_SYSTEM
  #define _A_SYSTEM   0x04
#endif
#ifndef _A_SUBDIR
  #define _A_SUBDIR   0x10
#endif
#ifndef _A_ARCH
  #define _A_ARCH     0x20
#endif

/* Watcom/MSVC function name mappings to POSIX equivalents */
#ifndef stricmp
  #define stricmp     strcasecmp
#endif
#ifndef strnicmp
  #define strnicmp    strncasecmp
#endif
#ifndef _strnicmp
  #define _strnicmp   strncasecmp
#endif

/* strlwr/strupr: in-place lowercase/uppercase (not in POSIX) */
static inline char *strlwr( char *s )
{
    char *p = s;
    while( *p ) { if( *p >= 'A' && *p <= 'Z' ) *p += 32; p++; }
    return s;
}

static inline char *strupr( char *s )
{
    char *p = s;
    while( *p ) { if( *p >= 'a' && *p <= 'z' ) *p -= 32; p++; }
    return s;
}

/* itoa/ltoa/utoa: integer-to-string (not in POSIX) */
static inline char *itoa( int value, char *buf, int radix )
{
    if( radix == 10 ) {
        sprintf( buf, "%d", value );
    } else if( radix == 16 ) {
        sprintf( buf, "%x", value );
    } else if( radix == 8 ) {
        sprintf( buf, "%o", value );
    } else {
        buf[0] = '0'; buf[1] = 0;
    }
    return buf;
}

static inline char *ltoa( long value, char *buf, int radix )
{
    if( radix == 10 ) {
        sprintf( buf, "%ld", value );
    } else if( radix == 16 ) {
        sprintf( buf, "%lx", value );
    } else {
        sprintf( buf, "%ld", value );
    }
    return buf;
}

static inline char *utoa( unsigned value, char *buf, int radix )
{
    if( radix == 10 ) {
        sprintf( buf, "%u", value );
    } else if( radix == 16 ) {
        sprintf( buf, "%x", value );
    } else {
        sprintf( buf, "%u", value );
    }
    return buf;
}

/* memicmp: case-insensitive memory compare (Watcom/MSVC extension) */
static inline int memicmp( const void *s1, const void *s2, size_t n )
{
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    size_t i;
    for( i = 0; i < n; i++ ) {
        int c1 = ( p1[i] >= 'A' && p1[i] <= 'Z' ) ? p1[i] + 32 : p1[i];
        int c2 = ( p2[i] >= 'A' && p2[i] <= 'Z' ) ? p2[i] + 32 : p2[i];
        if( c1 != c2 ) return c1 - c2;
    }
    return 0;
}

/* filelength: get file size by file descriptor (Watcom/MSVC extension) */
static inline long filelength( int fd )
{
    struct stat st;
    if( fstat( fd, &st ) < 0 ) return -1;
    return (long)st.st_size;
}

/* tell: get file position (Watcom/MSVC extension) */
static inline long tell( int fd )
{
    return (long)lseek( fd, 0, SEEK_CUR );
}

/* setmode: no-op on Linux (no text/binary distinction) */
#ifndef O_BINARY
  #define O_BINARY 0
#endif
#ifndef O_TEXT
  #define O_TEXT   0
#endif
static inline int setmode( int fd, int mode )
{
    (void)fd; (void)mode;
    return 0;
}

/* _MAX_PATH and friends (MSVC/Watcom constants) */
#ifndef _MAX_PATH
  #define _MAX_PATH     PATH_MAX
#endif
#ifndef _MAX_DRIVE
  #define _MAX_DRIVE    3
#endif
#ifndef _MAX_DIR
  #define _MAX_DIR      256
#endif
#ifndef _MAX_FNAME
  #define _MAX_FNAME    256
#endif
#ifndef _MAX_EXT
  #define _MAX_EXT      256
#endif

/* _splitpath / _makepath: path decomposition (Watcom/MSVC extension) */
static inline void _splitpath( const char *path, char *drive, char *dir,
                                char *fname, char *ext )
{
    const char *p, *last_sep, *last_dot;

    /* No drive letters on Linux */
    if( drive ) drive[0] = 0;

    /* Find last separator */
    last_sep = NULL;
    for( p = path; *p; p++ ) {
        if( *p == '/' ) last_sep = p;
    }

    /* Extract directory */
    if( dir ) {
        if( last_sep ) {
            int len = (int)(last_sep - path) + 1;
            memcpy( dir, path, len );
            dir[len] = 0;
        } else {
            dir[0] = 0;
        }
    }

    /* Point to filename part */
    p = last_sep ? last_sep + 1 : path;

    /* Find last dot in filename */
    last_dot = NULL;
    {
        const char *q;
        for( q = p; *q; q++ ) {
            if( *q == '.' ) last_dot = q;
        }
    }

    /* Extract filename and extension */
    if( last_dot && last_dot > p ) {
        if( fname ) {
            int len = (int)(last_dot - p);
            memcpy( fname, p, len );
            fname[len] = 0;
        }
        if( ext ) strcpy( ext, last_dot );
    } else {
        if( fname ) strcpy( fname, p );
        if( ext ) ext[0] = 0;
    }
}

static inline void _makepath( char *path, const char *drive, const char *dir,
                               const char *fname, const char *ext )
{
    path[0] = 0;
    (void)drive;  /* no drives on Linux */
    if( dir && dir[0] ) {
        strcat( path, dir );
    }
    if( fname && fname[0] ) {
        strcat( path, fname );
    }
    if( ext && ext[0] ) {
        if( ext[0] != '.' ) strcat( path, "." );
        strcat( path, ext );
    }
}

/* _cmdname: return path to current executable (Watcom extension) */
static inline char *_cmdname( char *buf )
{
    ssize_t len = readlink( "/proc/self/exe", buf, FILENAME_MAX - 1 );
    if( len > 0 ) {
        buf[len] = 0;
    } else {
        strcpy( buf, "vi" );
    }
    return buf;
}

/* OW stdui border/line-drawing character constants.
 * The OW UI library defines these for its TUI framework; we provide
 * simple ASCII equivalents since ncurses ACS_* macros are runtime
 * values and can't be used in static initializers. */
#define UI_ULCORNER     '+'
#define UI_URCORNER     '+'
#define UI_LLCORNER     '+'
#define UI_LRCORNER     '+'
#define UI_VLINE        '|'
#define UI_HLINE        '-'
#define UI_LTEE         '+'
#define UI_RTEE         '+'
#define UI_BLOCK        '#'
#define UI_CKBOARD      ':'
#define UI_UDARROW      '|'
#define UI_UPOINT       '^'
#define UI_DPOINT       'v'
#define UI_EQUIVALENT   '='

/* _fullpath: resolve a relative path to an absolute path (Watcom/MSVC) */
static inline char *_fullpath( char *buf, const char *path, size_t maxlen )
{
    if( buf == NULL ) {
        return realpath( path, NULL );
    }
    if( realpath( path, buf ) != NULL ) {
        return buf;
    }
    /* realpath fails if file doesn't exist yet; fall back to just copying */
    if( path[0] == '/' ) {
        snprintf( buf, maxlen, "%s", path );
    } else {
        char cwd[PATH_MAX];
        if( getcwd( cwd, sizeof( cwd ) ) != NULL ) {
            snprintf( buf, maxlen, "%s/%s", cwd, path );
        } else {
            snprintf( buf, maxlen, "%s", path );
        }
    }
    return buf;
}

/* _searchenv: search for a file along a path env variable (Watcom/MSVC).
 * Looks for 'name' in each directory listed in the environment variable
 * 'env_var' (colon-separated on Linux). If found, copies full path to 'buf'. */
static inline void _searchenv( const char *name, const char *env_var, char *buf )
{
    const char  *env, *p;
    char        trial[PATH_MAX];

    buf[0] = 0;
    env = getenv( env_var );
    if( env == NULL ) {
        return;
    }
    while( *env ) {
        p = env;
        while( *p && *p != ':' ) p++;
        if( p > env ) {
            int len = (int)(p - env);
            memcpy( trial, env, len );
            trial[len] = '/';
            strcpy( trial + len + 1, name );
            if( access( trial, F_OK ) == 0 ) {
                strcpy( buf, trial );
                return;
            }
        }
        env = ( *p == ':' ) ? p + 1 : p;
    }
}

/* ultoa: unsigned long to string (not in POSIX) */
static inline char *ultoa( unsigned long value, char *buf, int radix )
{
    if( radix == 10 ) {
        sprintf( buf, "%lu", value );
    } else if( radix == 16 ) {
        sprintf( buf, "%lx", value );
    } else {
        sprintf( buf, "%lu", value );
    }
    return buf;
}

/* Watcom C runtime globals _argc/_argv: on Linux, set by main() */
extern int  _argc;
extern char **_argv;

/* sopen: file open with sharing (no-op sharing on Linux, just use open) */
#ifndef sopen
  #define sopen( path, oflag, shflag, ... ) open( path, oflag, ##__VA_ARGS__ )
#endif
#ifndef SH_COMPAT
  #define SH_COMPAT  0
#endif
#ifndef SH_DENYNO
  #define SH_DENYNO  0
#endif
#ifndef SH_DENYRD
  #define SH_DENYRD  0
#endif
#ifndef SH_DENYWR
  #define SH_DENYWR  0
#endif

/* _msize: get size of malloc'd block (Watcom/MSVC extension) */
/* Linux has malloc_usable_size in <malloc.h>, but it returns a
 * potentially larger value. Good enough for the editor's usage. */
#include <malloc.h>
#ifndef _msize
  #define _msize(p) malloc_usable_size(p)
#endif

#endif /* _CLIBEXT_H_INCLUDED */
