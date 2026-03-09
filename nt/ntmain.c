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
* Description:  WHEN YOU FIGURE OUT WHAT THIS FILE DOES, PLEASE
*               DESCRIBE IT HERE!
*
****************************************************************************/


#include "vi.h"
#include "source.h"
#include <windows.h>

static char exePathBuf[MAX_PATH];

int main( int argc, char *argv[] )
{
#ifdef TRMEM
    InitTRMEM();
#endif

    (void)argc;
    /* Get the full path to our own executable so CheckForBoundData
     * can open it to read appended data.  argv[0] is unreliable —
     * it may be just "vi" with no path. */
    if( GetModuleFileNameA( NULL, exePathBuf, MAX_PATH ) > 0 ) {
        EXEName = exePathBuf;
    } else {
        EXEName = argv[0];
    }
    EditFlags.HasSystemMouse = TRUE;
    VarAddGlobalStr( "OS", "nt" );
    Comspec = getenv( "ComSpec" );
    InitializeEditor();
    EditMain();

#ifdef TRMEM
    DumpTRMEM();
#endif

    return( 0 );

} /* main */
