/****************************************************************************
*
*  autoenv.c -- Stub for Open Watcom auto-environment module.
*
*  The real autoenv.c from Open Watcom automatically sets up environment
*  variables from a configuration file. For the MSVC build this is a no-op
*  stub that satisfies the linker.
*
****************************************************************************/

void watcom_setup_env( void )
{
    /* No-op: autoenv functionality not needed under modern Windows */
}
