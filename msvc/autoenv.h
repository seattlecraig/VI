/****************************************************************************
*
*  autoenv.h -- Stub for Open Watcom auto-environment header.
*
*  The real autoenv module from Open Watcom automatically sets up
*  environment variables. For the MSVC build this is a no-op.
*
****************************************************************************/

#ifndef _AUTOENV_H_INCLUDED
#define _AUTOENV_H_INCLUDED

/* No-op stub: autoenv functionality not needed under modern Windows */
void watcom_setup_env( void );

#endif
