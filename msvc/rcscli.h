/****************************************************************************
*
*  rcscli.h -- Stub for Open Watcom RCS DLL client interface.
*
*  The real rcscli.h from the Open Watcom tree defines function pointer
*  types and constants for dynamically loading the RCS DLL. This stub
*  provides just enough for rcs.c and cmdline.c to compile under MSVC.
*
*  VI_RCS is NOT defined for non-Watcom compilers (see vi.h), so most
*  RCS code paths are compiled out. However, rcs.c still needs the
*  type definitions for the function pointer declarations that are
*  conditionally compiled under __NT__.
*
****************************************************************************/

#ifndef _RCSCLI_H_INCLUDED
#define _RCSCLI_H_INCLUDED

/* Calling convention for RCS DLL functions */
#ifndef RCSAPI
  #define RCSAPI __cdecl
#endif

/* RCS DLL name */
#define RCS_DLLNAME     "rcsdll.dll"

/* Function name strings used by GetProcAddress */
#define GETVER_FN_NAME          "RCSGetVersion"
#define INIT_FN_NAME            "RCSInit"
#define CHECKOUT_FN_NAME        "RCSCheckout"
#define CHECKIN_FN_NAME         "RCSCheckin"
#define HAS_SHELL_FN_NAME       "RCSHasShell"
#define RUNSHELL_FN_NAME        "RCSRunShell"
#define SETSYS_FN_NAME          "RCSSetSystem"
#define GETSYS_FN_NAME          "RCSQuerySystem"
#define REG_BAT_CB_FN_NAME      "RCSRegisterBatchCallback"
#define REG_MSGBOX_CB_FN_NAME   "RCSRegisterMessageBoxCallback"
#define SET_PAUSE_FN_NAME       "RCSSetPause"
#define FINI_FN_NAME            "RCSFini"

/* Callback function pointer type for batch operations */
typedef int (RCSAPI *BatchCallbackFP)( char *cmd, void *cookie );

/* RCS DLL function pointer types */
typedef int     (RCSAPI RCSGetVersionFn)( void );
typedef long    (RCSAPI RCSInitFn)( unsigned long, const char * );
typedef int     (RCSAPI RCSCheckoutFn)( long, const char *, const char *, const char * );
typedef int     (RCSAPI RCSCheckinFn)( long, const char *, const char *, const char * );
typedef int     (RCSAPI RCSHasShellFn)( long );
typedef int     (RCSAPI RCSRunShellFn)( long );
typedef int     (RCSAPI RCSSetSystemFn)( long, int );
typedef int     (RCSAPI RCSQuerySystemFn)( long );
typedef int     (RCSAPI RCSRegBatchCbFn)( long, BatchCallbackFP, void * );
typedef int     (RCSAPI RCSRegMsgBoxCbFn)( long, void *, void * );
typedef int     (RCSAPI RCSSetPauseFn)( long, int );
typedef int     (RCSAPI RCSFiniFn)( long );

#endif
