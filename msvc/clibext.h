/****************************************************************************
*
*  clibext.h -- C library extensions stub for MSVC builds.
*
*  The real clibext.h from Open Watcom provides POSIX-like functions
*  that are missing or renamed in non-Watcom compilers. For MSVC,
*  most of these exist under different names or are already available.
*
****************************************************************************/

#ifndef _CLIBEXT_H_INCLUDED
#define _CLIBEXT_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <direct.h>
#include <process.h>
#include <dos.h>    /* pulls in msvc/dos.h for DIR, opendir, etc. */

/* POSIX access() mode constants (MSVC doesn't define F_OK) */
#ifndef F_OK
  #define F_OK  0
  #define W_OK  2
  #define R_OK  4
#endif

/* MSVC equivalents for common POSIX/Watcom functions */
#ifndef stricmp
  #define stricmp   _stricmp
#endif
#ifndef strnicmp
  #define strnicmp  _strnicmp
#endif
#ifndef strlwr
  #define strlwr    _strlwr
#endif
#ifndef strupr
  #define strupr    _strupr
#endif
#ifndef getcwd
  #define getcwd    _getcwd
#endif
#ifndef chdir
  #define chdir     _chdir
#endif
#ifndef mkdir
  #define mkdir     _mkdir
#endif
#ifndef access
  #define access    _access
#endif
#ifndef unlink
  #define unlink    _unlink
#endif
#ifndef fileno
  #define fileno    _fileno
#endif
#ifndef isatty
  #define isatty    _isatty
#endif
#ifndef open
  #define open      _open
#endif
#ifndef close
  #define close     _close
#endif
#ifndef read
  #define read      _read
#endif
#ifndef write
  #define write     _write
#endif
#ifndef lseek
  #define lseek     _lseek
#endif
#ifndef tell
  #define tell      _tell
#endif
#ifndef dup
  #define dup       _dup
#endif
#ifndef dup2
  #define dup2      _dup2
#endif
#ifndef putenv
  #define putenv    _putenv
#endif
#ifndef memicmp
  #define memicmp   _memicmp
#endif
#ifndef itoa
  #define itoa      _itoa
#endif
#ifndef ltoa
  #define ltoa      _ltoa
#endif
#ifndef filelength
  #define filelength _filelength
#endif
#ifndef setmode
  #define setmode   _setmode
#endif
#ifndef umask
  #define umask     _umask
#endif
#ifndef stat
  #define stat      _stat
#endif
#ifndef fstat
  #define fstat     _fstat
#endif

/* Watcom's setenv(name, value, overwrite) -- MSVC doesn't have it */
static __inline int setenv( const char *name, const char *value, int overwrite )
{
    if( !overwrite ) {
        size_t len;
        if( getenv_s( &len, NULL, 0, name ) == 0 && len > 0 )
            return 0; /* already set, don't overwrite */
    }
    return _putenv_s( name, value );
}

/* Watcom utoa() -- MSVC has _ultoa but not utoa */
#ifndef utoa
  #define utoa( value, buf, radix ) _ultoa( (unsigned long)(value), (buf), (radix) )
#endif

/* Watcom C runtime globals _argc/_argv -- MSVC uses __argc/__argv */
#ifndef _argc
  #define _argc __argc
#endif
#ifndef _argv
  #define _argv __argv
#endif

#endif
