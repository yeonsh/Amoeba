/*	@(#)counter.c	1.5	96/02/27 13:58:06 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * The sun4m counter driver
 */

#include "amoeba.h"
#include "machdep.h"
#include "interrupt.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "openprom.h"
#include "sys/proto.h"
#include "randseed.h"
#include "memaccess.h"
#include "stderr.h"
#include "kerneldir.h"
#include "process/proc.h"


#define HW_TICKS_PER_SECOND	2000000
#define TICKS_PER_SECOND	100

#ifdef PROFILE
/* variable telling if we're currently profiling: */
static int now_profiling;

#define PROF_TICKS_PER_SECOND	50 /* user profile code depends on this */
#if (TICKS_PER_SECOND % PROF_TICKS_PER_SECOND) != 0
 #error "TICKS_PER_SECOND must be a multiple of PROF_TICKS_PER_SECOND"
#endif
#endif

struct proc_counter {
	volatile uint32	pc_limit;
	volatile uint32	pc_counter;
	uint32	pc_nrlimit;
	uint32	pc_ustartstop;
};
#define pc_umsw pc_limit
#define pc_ulsw pc_counter

struct syst_counter {
	volatile uint32	sc_limit;
	volatile uint32	sc_counter;
	uint32	sc_nrlimit;
	uint32	sc_reserved;
	uint32	sc_timerconfreg;
};

#define COUNTERSHIFT		9
#define LIMIT_REACHED_BIT	0x80000000
#define COUNTER_MASK		0x7FFFFE00

static struct {
	struct	proc_counter	*ca_pc;
	struct	syst_counter	*ca_sc;
} ca;
static dev_reg_t caphys[2];
static counter_found;

static void
set_counter_addr(p)
nodelist *p;
{
	int proplen, n;

	assert(!counter_found);
	proplen = obp_getattr(p->l_curnode, "address",
						(void *) &ca, sizeof(ca));
	compare (proplen, ==, sizeof(ca));
	DPRINTF(1, ("counters @ %x and %x\n", ca.ca_pc, ca.ca_sc));
	n = obp_n_devaddr(p, caphys, 2);
	compare (n, ==, 2);
	/* temp hack: */
	DPRINTF(1, ("and physical %x %x %x\n",
		    caphys[0].reg_bustype, caphys[0].reg_addr, caphys[0].reg_size));

	counter_found = 1;
}

#ifdef PROFILE

/* We temporily increase the rate of the system counter during profiling. */

/*ARGSUSED*/
void
prof_start_clock(milli)
interval milli;  /* want tick every "milli" millisecs; ignored for now */
{
	now_profiling = 1;
}

void
prof_stop_clock()
{
	now_profiling = 0;
}

#endif /* PROFILE */

/* milli_uptime must be volatile, or the compiler may change the order
 * of initialization in the timer_milli routine.
 */
static volatile unsigned long milli_uptime;


static unsigned long
timer_milli()
{
	unsigned long uptime, microtm, millitm;

	/*
	 * Let us hope there are no races in this code
	 */
	uptime = milli_uptime;
	microtm = ca.ca_sc->sc_counter;
	millitm = (microtm & COUNTER_MASK) >> COUNTERSHIFT;
	millitm /= HW_TICKS_PER_SECOND / 1000;
	if ((microtm & LIMIT_REACHED_BIT) ||
	    (millitm < 4 && milli_uptime > uptime))
	{
		DPRINTF(1, ("timer_milli: mup %d, up %d, ut x%lx, mt x%lx\n",
			    milli_uptime, uptime, microtm, millitm));
		/* If millitm is very small and milli_uptime is nevertheless
		   larger than uptime, we assume that there was an
		   "intpcounter" interrupt between the initialization of
		   uptime and the initialization of millitm.
		 */
		uptime += 1000L/TICKS_PER_SECOND;
	}
	return uptime + millitm;
}

/*ARGSUSED*/
static void
intpcounter( tt, sp, pc, psr, pid )
int tt;                                 /* Should be INTERUPT( timer ) */
thread_ustate *sp;                      /* thread being interrupted */
int pc;                                 /* ... and it's pc */
int psr;                                /* ... and it's psr */
int pid;                                /* Process ID number */
{
	(void) mem_get_long(&ca.ca_sc->sc_limit);

#ifdef PROFILE
	if (now_profiling) {
	    extern void profile_timer();
	    static int clock_ticks;

	    /* The sweeper is run at a higher rate than the profiler */
	    if (++clock_ticks == TICKS_PER_SECOND / PROF_TICKS_PER_SECOND) {
		profile_timer( sp, (uint32) pc, (uint32) psr );
		clock_ticks = 0;
	    }
	}
#endif
	enqueue( sweeper_run, 1000L/TICKS_PER_SECOND );
	milli_uptime += 1000L/TICKS_PER_SECOND;
}
	
static void
unexpected_interrupt()
{
	/* Got an unexpected timer interrupt. Clear the limit register
	 * to get a delay as long as possible.  Another possibility
	 * would be to ignore this interrupt altogether, but that
	 * may be a lot more work.
	 */
	(void) mem_get_long(&ca.ca_pc->pc_limit);
	ca.ca_pc->pc_limit = 0;

	DPRINTF(1, ("OOPS: timer interrupt?!\n"));
}

void
inittimer()
{
	extern void set_hw_milli();

	obp_regnode("name", "counter", set_counter_addr);
	assert(counter_found);
	(void) setvec(INT_CNTR0, intpcounter);

	/*
	 * read bits for random numbers before resetting counter
	 */
	randseed((unsigned long) ca.ca_sc->sc_counter>>COUNTERSHIFT,
			20, RANDSEED_INVOC);
	/*
	 * Supply better timing function for sys_milli
	 */
	set_hw_milli(timer_milli);

	/*
	 * Setting limit resets counter to 1
	 */
	ca.ca_sc->sc_limit =
	    (HW_TICKS_PER_SECOND / TICKS_PER_SECOND) << COUNTERSHIFT;

	/* Catch unexpected interrupts from the timer we don't use.
	 * If we configure it as user timer, it should never interrupt anyway.
	 */
	(void) setvec(INT_CNTR1, unexpected_interrupt);

#ifndef NO_USERTIMER
	/* Configure the Processor Counter as user timer.
	 * Note that this is done by means of a field in the System Counter.
	 */
	ca.ca_sc->sc_timerconfreg = 1;	/* configure user timer */
	ca.ca_pc->pc_ustartstop = 1;	/* start counting */
#else
	ca.ca_pc->pc_limit = 0;
#endif
}

#ifndef NO_USERTIMER
#define TIMER_RIGHTS	(PSR_READ | PSR_WRITE)

void
initusertimer()
{
	/* Set up a segment for the user timer, so that it can be
	 * mapped in by a user process.  Unfortunately we'll have
	 * to map in more than we need (or wish) because of the
	 * segment boundary restrictions.
	 */
	capability timer_cap;
	static long timer_cols[NROOTCOLUMNS] =
					 { 0xFF, TIMER_RIGHTS, TIMER_RIGHTS };
	errstat err;

	err = HardwareSeg(&timer_cap, (phys_bytes) caphys[0].reg_addr,
			  (phys_bytes) CLICKSIZE, 1);
	if (err != STD_OK) {
	    printf("initusertimer failed (%s)\n", err_why(err));
	} else {
	    err = dirappend("usertimer", &timer_cap);
	    if (err != STD_OK) {
		printf("cannot install user timer (%s)\n", err_why(err));
	    } else {
		err = dirchmod("usertimer", NROOTCOLUMNS, timer_cols);
		if (err != STD_OK) {
		    printf("cannot chmod user timer (%s)\n", err_why(err));
		}
	    }
	}
}

#ifdef MEASURE
unsigned long
getmicro()
{
#ifdef __GNUC__
	union {
	    long long tm_all;	/* all 64 bits */
	    struct {
		uint32 tm_msw;
		uint32 tm_lsw;
	    } tm_parts;
	} timer_val;
	register volatile long long *timer;
	
	timer = (volatile long long *) &ca.ca_pc->pc_limit;
	timer_val.tm_all = *timer;
	/* convert to micro seconds (see lib/sysam/sparc/sun4m_timer.c) */
	return (timer_val.tm_parts.tm_lsw >> 10) |
	       (timer_val.tm_parts.tm_msw << (32 - 10));
#else
	/* requires assembly support to copy 64 bit timer value at once */
	return -1;
#endif
}
#endif /* MEASURE */
#endif /* NO_USERTIMER */

/*
 * Below is a busy wait routine for very short waits needed by device
 * drivers that have to poll things (like the floppy driver).
 */

void
microsec_delay(t)
unsigned long	t;
{
	unsigned long	base;
	unsigned long	current;

/*
	assert(t <= 10);
*/

	base = ca.ca_sc->sc_counter;
	base &= COUNTER_MASK;
	base >>= COUNTERSHIFT;

	do
	{
	    current = ca.ca_sc->sc_counter;
	    current &= COUNTER_MASK;
	    current >>= COUNTERSHIFT;
	} while (current - base < t);
}
