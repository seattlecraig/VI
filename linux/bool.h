/****************************************************************************
*
*  bool.h -- Stub for Open Watcom bool.h.
*
*  Watcom's bool.h provides the 'bool' type for C. GCC supports the
*  C99 <stdbool.h> which defines bool, true, and false.
*
****************************************************************************/

#ifndef _WATCOM_BOOL_H_COMPAT
#define _WATCOM_BOOL_H_COMPAT

#include <stdbool.h>

/* Watcom uses uppercase TRUE/FALSE throughout the codebase */
#ifndef TRUE
  #define TRUE  true
#endif
#ifndef FALSE
  #define FALSE false
#endif

#endif
