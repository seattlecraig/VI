/****************************************************************************
*
*  banner.h -- Stub banner header for MSVC builds.
*
*  Replaces the Open Watcom banner.h that lives in the Watcom tree.
*  Provides the macros used by version.c and other files.
*
****************************************************************************/

#ifndef _BANNER_H_INCLUDED
#define _BANNER_H_INCLUDED

/* Version string -- adjust as needed */
#define _VI_VERSION_    "3.0.0"

/* banner2( year ) produces a copyright string */
#define banner2( year )  "Copyright (c) 1991-" year " Open Watcom Contributors"

/* banner2a() used by bind/ctags, not needed for vi.exe itself */
#define banner2a()       "Copyright (c) Open Watcom Contributors"

/* banner1p2 used by windowed version only, stub it anyway */
#define banner1p2( ver ) "Version " ver

#endif
