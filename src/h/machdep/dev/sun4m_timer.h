/*	@(#)sun4m_timer.h	1.1	96/02/27 10:31:19 */
#ifndef _SUN4M_TIMER_H_
#define _SUN4M_TIMER_H_

/*
 * Definitions for the user level timer supported by sun4m kernels.
 */

#include <_ARGS.h>
#include <memaccess.h>

#ifdef __GNUC__
#define pc_timer_type	volatile long long
#else
#define pc_timer_type	volatile double	/* forcing 8 byte alignment */
#endif

struct sun4m_timer_regs {
    union sun4m_timer {
	pc_timer_type pc__timer_val;		/* full 64 bit value */
	struct {
	    volatile uint32 pc__timer_msw;	/* most significant 32 bits */
	    volatile uint32 pc__timer_lsw;	/* least significant bits */
	} pc__words;
    } pc__timer;
    uint32  pc_nrlimit;				/* limit, not useful here */
    uint32  pc_ustartstop;			/* 0 stops, 1 starts timer */
};

#define tm_val		pc__timer_val
#define pc_timer	pc__timer.tm_val
#define tm_msw		pc__words.pc__timer_msw
#define tm_lsw		pc__words.pc__timer_lsw
#define pc_timer_msw	pc__timer.tm_msw
#define pc_timer_lsw	pc__timer.tm_lsw


/* For consistency, the timer's msw and lsw should be fetched with a
 * single 8 byte access. With GNU this happens automatically as a result
 * of the 'long long' specification.  For other compilers, _copy_int64()
 * can be used to do the job.
 */
#ifdef __GNUC__
#define copy_timer(srcp,destp)	*destp = *srcp
#else
#define copy_timer(srcp,destp)	_copy_int64(srcp, destp)
#endif

errstat sun4m_timer_init   _ARGS((char *_hostname));

struct  sun4m_timer_regs *
        sun4m_timer_getptr _ARGS((void));

long    sun4m_timer_diff   _ARGS((union sun4m_timer *_start,
				  union sun4m_timer *_stop));

uint32  sun4m_timer_micro  _ARGS((void));

#endif /* _SUN4M_TIMER_H_ */
