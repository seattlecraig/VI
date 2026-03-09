/****************************************************************************
*
* Description:  Linux signal/timer handling.
*
*               Provides SetInterrupts() and RestoreInterrupts() for the
*               editor's timer tick and break-key handling.
*
****************************************************************************/

#include "vi.h"
#include <signal.h>
#include <sys/time.h>

/*
 * handleSigint - handle Ctrl+C / SIGINT
 */
static void handleSigint( int sig )
{
    (void)sig;
    if( EditFlags.WatchForBreak ) {
        EditFlags.BreakPressed = TRUE;
    }
}

/*
 * handleSigalrm - periodic timer handler for clock ticks
 */
static void handleSigalrm( int sig )
{
    (void)sig;
    ClockTicks++;
}

/*
 * SetInterrupts - install signal handlers for timer and break
 */
void SetInterrupts( void )
{
    struct itimerval timer;
    struct sigaction sa;

    /* SIGINT handler for Ctrl+C */
    memset( &sa, 0, sizeof( sa ) );
    sa.sa_handler = handleSigint;
    sa.sa_flags = SA_RESTART;
    sigaction( SIGINT, &sa, NULL );

    /* Ignore SIGPIPE (can happen when piping to external commands) */
    signal( SIGPIPE, SIG_IGN );

    /* SIGALRM for periodic clock ticks (~18.2 Hz like DOS, ~55ms) */
    sa.sa_handler = handleSigalrm;
    sa.sa_flags = SA_RESTART;
    sigaction( SIGALRM, &sa, NULL );

    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 55000;     /* 55ms */
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 55000;  /* 55ms */
    setitimer( ITIMER_REAL, &timer, NULL );
}

/*
 * RestoreInterrupts - restore default signal handlers
 */
void RestoreInterrupts( void )
{
    struct itimerval timer;

    /* Stop the timer */
    memset( &timer, 0, sizeof( timer ) );
    setitimer( ITIMER_REAL, &timer, NULL );

    /* Restore default handlers */
    signal( SIGINT, SIG_DFL );
    signal( SIGALRM, SIG_DFL );
    signal( SIGPIPE, SIG_DFL );
}
