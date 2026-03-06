/****************************************************************************
*
*  walloca.h -- Watcom alloca() compatibility for MSVC.
*
*  Watcom's walloca.h provides the alloca() function. MSVC provides
*  the same functionality via _alloca() in <malloc.h>.
*
****************************************************************************/

#ifndef _WALLOCA_H_COMPAT
#define _WALLOCA_H_COMPAT

#include <malloc.h>

#ifndef alloca
  #define alloca _alloca
#endif

#endif
