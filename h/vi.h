/****************************************************************************
*
*                            Open Watcom Project
*
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  Editor top level include file.
*
****************************************************************************/


#ifndef __VI_INCLUDED__
#define __VI_INCLUDED__

#include "control.h"

#ifndef _FAR
  #error _FAR not configured
#endif

#ifndef __WATCOMC__
  #include "clibext.h"
#endif

typedef unsigned int U_INT;

char            *_inline_strchr( const char *__s, int __c );
unsigned int    _inline_strlen( const char *__s );
int             _inline_strcmp( const char *__s1, const char *__s2 );
char            *_inline_strcat( char *__s1, const char *__s2 );
void            *_inline_memcpy( void *__s1, const void *__s2, unsigned int __n );
void            *_inline_memset( void *__s, int __c, unsigned int __n );

#ifdef __UNIX__
  #define FSYS_CASE_SENSITIVE         1
  #ifdef __QNX__
    extern int FileSysNeedsCR( int handle );
  #else
    #define FileSysNeedsCR( x )       0
  #endif
#else
  #define FSYS_CASE_SENSITIVE         0
  #define FileSysNeedsCR( x )         1
#endif

#if defined( __WATCOMC__ ) && !defined( __AXP__ ) && !defined( __PPC__ ) && !defined( __MIPS__ )
  #define VI_RCS  1
#endif

#include "const.h"
#include "errs.h"
#include "lang.h"
#include "struct.h"
#include "mouse.h"
#include "globals.h"
#include "rtns.h"
#include "rtns2.h"
#include "dc.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * MSVC debug CRT asserts if ctype functions receive values outside -1..255.
 * Signed chars > 127 go negative and trigger this. Cast to unsigned char
 * to make all ctype calls safe.
 */
#ifndef __WATCOMC__
  #undef isspace
  #undef isalpha
  #undef isdigit
  #undef isalnum
  #undef isupper
  #undef islower
  #undef isprint
  #undef isxdigit
  #undef toupper
  #undef tolower
  #define isspace( c )  isspace( (unsigned char)(c) )
  #define isalpha( c )  isalpha( (unsigned char)(c) )
  #define isdigit( c )  isdigit( (unsigned char)(c) )
  #define isalnum( c )  isalnum( (unsigned char)(c) )
  #define isupper( c )  isupper( (unsigned char)(c) )
  #define islower( c )  islower( (unsigned char)(c) )
  #define isprint( c )  isprint( (unsigned char)(c) )
  #define isxdigit( c ) isxdigit( (unsigned char)(c) )
  #define toupper( c )  toupper( (unsigned char)(c) )
  #define tolower( c )  tolower( (unsigned char)(c) )
#endif

#ifdef __WIN__
  #include "winvi.h"
#elif defined( __NT__ )
  #define _WINSOCKAPI_
  /* CR macro from const.h (0x0d) conflicts with winnt.h bitfield name */
  #undef CR
  #include <windows.h>
  #define CR 0x0d
#endif

#endif
