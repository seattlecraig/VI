/****************************************************************************
*
*  dos.h -- Linux stub for Watcom <dos.h>.
*
*  On Linux there are no DOS functions. The core code includes <dos.h>
*  in a few places but the actual DOS functions are only used in the
*  platform-specific dosdir.c (which is replaced by unix/unixdir.c on
*  Linux).  This header provides just enough stubs to compile.
*
****************************************************************************/

#ifndef _WATCOM_DOS_H_COMPAT
#define _WATCOM_DOS_H_COMPAT

/* struct find_t: only needed for dosdir.c which is NOT compiled on Linux.
 * Provide a minimal stub in case any header references it. */
struct find_t {
    char            reserved[21];
    unsigned char   attrib;
    unsigned short  wr_time;
    unsigned short  wr_date;
    unsigned long   size;
    char            name[260];
};

/* Stubs for _dos_findfirst/_dos_findnext — should never be called on Linux */
static inline unsigned _dos_findfirst( const char *p, unsigned a, struct find_t *b )
{
    (void)p; (void)a; (void)b;
    return 1; /* failure */
}

static inline unsigned _dos_findnext( struct find_t *b )
{
    (void)b;
    return 1; /* failure */
}

static inline unsigned _dos_findclose( struct find_t *b )
{
    (void)b;
    return 0;
}

#endif /* _WATCOM_DOS_H_COMPAT */
