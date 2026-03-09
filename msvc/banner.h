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

/* banner1w( product, version ) - one-line product banner used by edbind */
#define banner1w( product, ver ) product " Version " ver

/* banner3 / banner3a - additional banner lines used by edbind */
#define banner3     "All rights reserved."
#define banner3a    ""

/* edbind version */
#define _EDBIND_VERSION_  "3.0.0"

#endif
