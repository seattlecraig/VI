/****************************************************************************
*
*  dos.h -- MSVC compatibility shim for Watcom <dos.h> and DOS-style
*           directory functions.
*
*  Watcom's <dos.h> provides _dos_findfirst/_dos_findnext with struct
*  find_t, plus constants like _A_SUBDIR. MSVC provides similar
*  functionality through <io.h> with _findfirst/_findnext and struct
*  _finddata_t, but the interfaces differ.
*
*  This header bridges the gap so code written for Watcom compiles
*  under MSVC with minimal source changes.
*
****************************************************************************/

#ifndef _WATCOM_DOS_H_COMPAT
#define _WATCOM_DOS_H_COMPAT

#include <io.h>
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

/* ---- File attribute constants (match Watcom's <dos.h>) ---- */
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

/* ---- struct find_t (Watcom-compatible) ---- */
/* Watcom's _dos_findfirst uses this structure */
struct find_t {
    char            reserved[21];   /* reserved by DOS */
    unsigned char   attrib;         /* file attributes */
    unsigned short  wr_time;        /* time of last write */
    unsigned short  wr_date;        /* date of last write */
    unsigned long   size;           /* file size */
    char            name[260];      /* file name */
};

/* ---- _dos_findfirst / _dos_findnext (Watcom-compatible) ---- */
/* These wrap MSVC's _findfirst/_findnext to match Watcom's interface */

/* Internal state for mapping _findfirst handle to _dos_findnext calls */
static intptr_t __dos_find_handle = -1;

static __inline unsigned _dos_findfirst( const char *path, unsigned attr, struct find_t *buf )
{
    struct _finddata_t  fd;
    intptr_t            h;

    (void)attr; /* MSVC _findfirst doesn't filter by attribute the same way */
    h = _findfirst( path, &fd );
    if( h == -1 ) {
        return( 1 ); /* failure */
    }
    __dos_find_handle = h;
    buf->attrib = (unsigned char)fd.attrib;
    buf->size = (unsigned long)fd.size;
    buf->wr_time = 0;
    buf->wr_date = 0;
    strncpy( buf->name, fd.name, sizeof( buf->name ) - 1 );
    buf->name[sizeof( buf->name ) - 1] = '\0';
    return( 0 ); /* success */
}

static __inline unsigned _dos_findnext( struct find_t *buf )
{
    struct _finddata_t  fd;

    if( __dos_find_handle == -1 ) {
        return( 1 );
    }
    if( _findnext( __dos_find_handle, &fd ) != 0 ) {
        _findclose( __dos_find_handle );
        __dos_find_handle = -1;
        return( 1 );
    }
    buf->attrib = (unsigned char)fd.attrib;
    buf->size = (unsigned long)fd.size;
    buf->wr_time = 0;
    buf->wr_date = 0;
    strncpy( buf->name, fd.name, sizeof( buf->name ) - 1 );
    buf->name[sizeof( buf->name ) - 1] = '\0';
    return( 0 );
}

static __inline unsigned _dos_findclose( struct find_t *buf )
{
    (void)buf;
    if( __dos_find_handle != -1 ) {
        _findclose( __dos_find_handle );
        __dos_find_handle = -1;
    }
    return( 0 );
}

/* ---- Watcom-style opendir/readdir/closedir with d_attr etc. ---- */
/* Watcom's <direct.h> provides DIR and struct dirent with d_attr,   */
/* d_size, d_date, d_time fields. MSVC's dirent only has d_name.     */
/* We provide our own implementation here.                            */

/* Only define these if they haven't been defined elsewhere */
#ifndef _WATCOM_DIRENT_COMPAT
#define _WATCOM_DIRENT_COMPAT

/* Undefine any prior DIR/dirent if needed -- we provide our own */
/* Note: MSVC doesn't have opendir/readdir in its standard headers */

struct dirent {
    char            d_name[260];    /* file name */
    unsigned char   d_attr;         /* file attributes */
    unsigned long   d_size;         /* file size in bytes */
    unsigned short  d_date;         /* date of last write (DOS format) */
    unsigned short  d_time;         /* time of last write (DOS format) */
};

typedef struct {
    intptr_t            handle;     /* _findfirst handle */
    struct _finddata_t  finddata;   /* MSVC find data */
    struct dirent       ent;        /* current entry */
    int                 first;      /* flag: first entry not yet read */
    char                pattern[260]; /* search pattern */
    /* Watcom-compatible shortcut fields (populated by opendir/readdir) */
    char                d_name[260];
    unsigned char       d_attr;
    unsigned long       d_size;
    unsigned short      d_date;
    unsigned short      d_time;
} DIR;

static __inline DIR *opendir( const char *path )
{
    DIR     *d;
    char    pattern[260];
    int     len;

    d = (DIR *)malloc( sizeof( DIR ) );
    if( d == NULL ) return NULL;

    /* Build search pattern: path\*.* or path/*.* */
    strncpy( pattern, path, sizeof( pattern ) - 5 );
    pattern[sizeof( pattern ) - 5] = '\0';
    len = (int)strlen( pattern );
    /* Remove trailing slash/backslash if present */
    if( len > 0 && (pattern[len - 1] == '\\' || pattern[len - 1] == '/') ) {
        pattern[len - 1] = '\0';
        len--;
    }
    /* Check if the pattern already contains wildcards */
    if( strchr( pattern, '*' ) != NULL || strchr( pattern, '?' ) != NULL ) {
        /* Already a wildcard pattern, use as-is */
    } else {
        strcat( pattern, "\\*.*" );
    }
    strncpy( d->pattern, pattern, sizeof( d->pattern ) );

    d->handle = _findfirst( d->pattern, &d->finddata );
    if( d->handle == -1 ) {
        /* Check if path itself is a file (for MyGetFileSize usage) */
        d->handle = _findfirst( path, &d->finddata );
        if( d->handle == -1 ) {
            free( d );
            return NULL;
        }
    }
    d->first = 1;
    /* Populate Watcom-compatible shortcut fields from first entry */
    strncpy( d->d_name, d->finddata.name, sizeof( d->d_name ) - 1 );
    d->d_name[sizeof( d->d_name ) - 1] = '\0';
    d->d_attr = (unsigned char)d->finddata.attrib;
    d->d_size = (unsigned long)d->finddata.size;
    d->d_date = 0;
    d->d_time = 0;
    return d;
}

static __inline struct dirent *readdir( DIR *d )
{
    if( d == NULL ) return NULL;

    if( d->first ) {
        d->first = 0;
    } else {
        if( _findnext( d->handle, &d->finddata ) != 0 ) {
            return NULL;
        }
    }

    strncpy( d->ent.d_name, d->finddata.name, sizeof( d->ent.d_name ) - 1 );
    d->ent.d_name[sizeof( d->ent.d_name ) - 1] = '\0';
    d->ent.d_attr = (unsigned char)d->finddata.attrib;
    d->ent.d_size = (unsigned long)d->finddata.size;
    /* Convert time_t to DOS date/time format (approximate) */
    d->ent.d_date = 0;
    d->ent.d_time = 0;
    /* Also populate Watcom-compatible shortcut fields on DIR itself */
    strncpy( d->d_name, d->finddata.name, sizeof( d->d_name ) - 1 );
    d->d_name[sizeof( d->d_name ) - 1] = '\0';
    d->d_attr = (unsigned char)d->finddata.attrib;
    d->d_size = (unsigned long)d->finddata.size;
    d->d_date = 0;
    d->d_time = 0;
    return &d->ent;
}

static __inline int closedir( DIR *d )
{
    if( d == NULL ) return -1;
    if( d->handle != -1 ) {
        _findclose( d->handle );
    }
    free( d );
    return 0;
}

#endif /* _WATCOM_DIRENT_COMPAT */

#endif /* _WATCOM_DOS_H_COMPAT */
