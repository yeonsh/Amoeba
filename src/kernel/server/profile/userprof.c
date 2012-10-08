/*	@(#)userprof.c	1.3	96/02/27 14:20:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define MPX
#include "amoeba.h"
#include "stderr.h"
#include "machdep.h"
#include "global.h"
#include "exception.h"
#include "kthread.h"
#include "debug.h"
#include "proto.h"
#include "module/profiling.h"

#define PROFILE_GRANULARITY	20L /* default: 1 tick every 50 msec */

extern errstat user_prof_start();
extern void user_prof_stop();

/*
 * Profiling support for user programs.
 *
 * Note: currently we only register ticks while a process is in user mode.
 * We could also try to keep system time statistics, but in a client/server
 * based system this makes much less sense than in a traditional standalone
 * workstation environment.  In general most of the time will be spent at
 * the (remote) server, while the client is just idle waiting for a reply.
 *
 * If the client is really interested in the time taken for various kinds
 * of RPCs, it is advisable to measure them separately using sys_milli().
 */

/* We're using 16 bit profile counters for compatibility with gprof: */
typedef unsigned short prof_count;

static void
user_prof_bill(pc, ticks)
vir_bytes pc;
unsigned long ticks;	/* # ticks to add */
{
    /* If the current user program is running with profiling on,
     * update its statistics.
     */
    register struct process *curproc = curthread->mx_process; 
    register prof_count *profbuf;

    if (curproc->pr_prof_buf == 0) {
	DPRINTF(1, ("prof_tick: %d not profiled\n", THREADSLOT(curthread)));
	return;
    }

    profbuf = (prof_count *) umap(curthread, curproc->pr_prof_buf,
				  curproc->pr_prof_size, 1);
    if (profbuf != 0) {
	/* If the current PC falls in the profiling range,
	 * increment the corresponding counter in the buffer.
	 */
	register vir_bytes pc_low;
	register int index;

	pc_low = curproc->pr_prof_pclow;
	if (pc >= pc_low) {
	    index = (((pc - pc_low) >> 2) * curproc->pr_prof_scale) >> 16;
	} else {
	    index = -1;
	}

	if (index >= 0 &&
	    index < curproc->pr_prof_size / sizeof(prof_count))
	{
	    profbuf[index] += ticks;
	    DPRINTF(1, ("prof_tick: %d: %lx (%d) -> %d\n",
			THREADSLOT(curthread), pc, index, profbuf[index]));
	}
	else
	{
	    DPRINTF(0, ("prof_tick: %d: %lx (index %d > %d)\n",
			THREADSLOT(curthread), pc, index,
			curproc->pr_prof_size / sizeof(prof_count)));
	}
    } else {
	DPRINTF(0, ("prof_tick: invalid buffer for %d\n",
		    THREADSLOT(curthread)));
	/* stop profiling this process */
	curproc->pr_prof_buf = 0;
    }
}

void
user_prof_tick(pc)
vir_bytes pc;
{
    user_prof_bill(pc, (unsigned long) 1);
}

unsigned long
user_prof_sys_start()
{
    /* return current time if current process is profiling */
    register struct process *curproc = curthread->mx_process; 

    if (curproc->pr_prof_buf != 0) {
	return getmilli();
    } else {
	return 0;
    }
}

void
user_prof_sys_finish(pc, start)
vir_bytes pc;
unsigned long start;	/* time the system call started */
{
    /* bill process for the system call */
    user_prof_bill(pc, (getmilli() - start) / PROFILE_GRANULARITY);
}

errstat
user_profile(arg)
struct profile_parms *arg;
{
    /* Start or stop user-level profiling for the calling program. */
    phys_bytes profile_buf;
    register struct process *curproc = curthread->mx_process; 

    switch (arg->prof_opcode) {
    case PROF_OPC_START:
	/* Check parameters */
	profile_buf = umap(curthread, (vir_bytes) arg->prof_buf,
			   (vir_bytes) arg->prof_bufsize, 1);
	if (profile_buf == 0) {
	    /* invalid buffer */
	    progerror();
	    return STD_ARGBAD;
	}
	if (((long) arg->prof_buf & (sizeof(prof_count) - 1)) != 0) {
	    /* unaligned buffer */
	    progerror();
	    return STD_ARGBAD;
	}
	
	if (curproc->pr_prof_buf != 0) {
	    /* we were already profiling */
	    return STD_NOTNOW;
	}

	/* Copy the relevant parameters to the process structure
	 * Note that we save the un-umapped pointer, because the
	 * program may be naughty and unmap a segment behind our back.
	 * For safety we umap the pointer on every clock tick.
	 */
	curproc->pr_prof_buf = (vir_bytes) arg->prof_buf;
	curproc->pr_prof_size = (vir_bytes) arg->prof_bufsize;
	curproc->pr_prof_pclow = (vir_bytes) arg->prof_pc_low;
	curproc->pr_prof_scale = arg->prof_scale;

	/* pr_prof_rate is ignored for now; the idea is to make the profiling
	 * rate user configurable, but the presence of multiple users of
	 * the profile timer might make this a bit difficult to implement.
	 */
	return user_prof_start(PROFILE_GRANULARITY);

    case PROF_OPC_STOP:
	/* Disable profiling for this program.  If this is the only
	 * user of the profile_timer, disable the timer as well.
	 */
	user_prof_stop();
	curproc->pr_prof_buf = 0;
	return STD_OK;

    default:
	return STD_ARGBAD;
    }
}
