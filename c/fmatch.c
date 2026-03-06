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
* Description:  Wildcard file matching.
*
****************************************************************************/


#include "vi.h"
#include <ctype.h>

static char *cWild;

/*
 * wildMatch - simple recursive wildcard matching
 *
 * Supports '*' (match zero or more chars) and '?' (match one char).
 * Case-insensitive on non-Unix platforms.
 */
static bool wildMatch( const char *pattern, const char *name )
{
    while( *pattern ) {
        if( *pattern == '*' ) {
            pattern++;
            /* skip consecutive stars */
            while( *pattern == '*' ) {
                pattern++;
            }
            if( *pattern == '\0' ) {
                return( TRUE );
            }
            while( *name ) {
                if( wildMatch( pattern, name ) ) {
                    return( TRUE );
                }
                name++;
            }
            return( FALSE );
        }
        if( *name == '\0' ) {
            return( FALSE );
        }
        if( *pattern == '?' ) {
            /* '?' matches any single character */
        } else {
#ifdef __UNIX__
            if( *pattern != *name ) {
                return( FALSE );
            }
#else
            if( tolower( (unsigned char)*pattern ) != tolower( (unsigned char)*name ) ) {
                return( FALSE );
            }
#endif
        }
        pattern++;
        name++;
    }
    return( *name == '\0' );
}

/*
 * FileMatch - check if a file matches a wild card
 */
bool FileMatch( char *name )
{
    return( wildMatch( cWild, name ) );

} /* FileMatch */

/*
 * FileMatchInit - start file matching
 */
vi_rc FileMatchInit( char *wild )
{
    cWild = MemAlloc( strlen( wild ) + 1 );
    strcpy( cWild, wild );
    return( ERR_NO_ERR );

} /* FileMatchInit */

/*
 * FileMatchFini - done with file matching
 */
void FileMatchFini( void )
{
    MemFree( cWild );
    cWild = NULL;

} /* FileMatchFini */
