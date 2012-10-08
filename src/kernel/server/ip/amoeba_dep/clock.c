/*	@(#)clock.c	1.4	96/02/27 14:14:10 */
/*
clock.c

Copyright 1994 Philip Homburg
*/

#include "inet.h"
#include "amoeba_incl.h"
#include "args.h"

#include "generic/buf.h"
#include "generic/type.h"
#include "generic/assert.h"
#include "generic/clock.h"

INIT_PANIC();

static void clck_fast_release ARGS(( timer_t *timer ));
static void ring_timer ARGS(( void ));
static void timer_loop ARGS(( void ));
static void print_timer_chain ARGS(( timer_t *chain ));
static void set_timer ARGS(( void ));

#define CLCK_TIMER_STACK	8192

static unsigned long current_era;
static timer_t *timer_chain;
static time_t next_timeout;
static semaphore sema_timer;

#define ERA_SHIFT	(sizeof(long) * 8 - ERA_BITS)

time_t get_time()
{
#ifdef DEBUG
	static time_t last_retval;
#endif
	static unsigned long last_sys_milli;
	unsigned long new_sys_milli;
	time_t retval;
	
#if DEBUG & 0
	{
		int x;
		if ((x = getsr()) & 0x700) {
			printf("interrupts disabled 0x%X\n", x);
			stacktrace();
		}
	}
#endif
	new_sys_milli = sys_milli();
	if (new_sys_milli < last_sys_milli) {
		/* timer wraparound */
#ifdef DEBUG
printf("ipsvr:get_time: timer wrapped: old era = 0x%X\nlsm = 0x%X, nsm = 0x%X lrv = 0x%X\n",
current_era, last_sys_milli, new_sys_milli, last_retval);
#endif
		current_era++;
	}
	last_sys_milli = new_sys_milli;

	/* Now try to extend the life of the clock before wrap around */
	new_sys_milli >>= ERA_BITS;
	new_sys_milli &= ~ERA_MASK;
	retval = (current_era << ERA_SHIFT) | new_sys_milli;

#ifdef DEBUG
	compare(retval, >=, last_retval);
	last_retval = retval;
#endif /* NDEBUG */
	return retval;
}

void clck_init ()
{
	next_timeout= 0;
	timer_chain= 0;
	sema_init(&sema_timer, 0);

	new_thread(timer_loop, CLCK_TIMER_STACK);
}

static void timer_loop()
{
	time_t curr_tim;
	interval delay;
	int n;

	mu_lock(&mu_generic);
	for (;;)
	{
		curr_tim= get_time();
		if (next_timeout && next_timeout <= curr_tim)
		{
			next_timeout= 0;
			ring_timer();
		}
		else
		{
			if (next_timeout)
			{
				assert (next_timeout>curr_tim);
				delay= (next_timeout - curr_tim) << ERA_BITS;
			}
			else
				delay= -1;
			
			n= sema_level(&sema_timer);
			if (n)
				sema_mdown(&sema_timer, n);
			mu_unlock(&mu_generic);
			(void)sema_trydown(&sema_timer, delay);
			mu_lock(&mu_generic);
		}
	}
}

static void print_timer_chain(chain)
timer_t *chain;
{
	if (!chain)
	{
		printf("(null)");
		return;
	}
	for (;chain; chain= chain->tim_next)
	{
		printf("{ ref= %d, timer= %d, f= 0x%x }%s", chain->tim_ref,
			chain->tim_time, chain->tim_func, chain->tim_next ?
			" -> " : "");
	}
}

PRIVATE void set_timer()
{
	time_t new_time;

	if (!timer_chain)
		return;

	new_time= timer_chain->tim_time;

	if (next_timeout == 0 || new_time < next_timeout)
	{
		next_timeout= new_time;
		sema_up(&sema_timer);
	}
}

PUBLIC void clck_untimer (timer)
timer_t *timer;
{
	clck_fast_release (timer);
	set_timer();
}

PRIVATE void ring_timer()
{
	time_t curr_time;
	timer_t *timer_index;

	if (timer_chain == NULL)
		return;

	curr_time= get_time();
	while (timer_chain && timer_chain->tim_time<=curr_time)
	{
		assert(timer_chain->tim_active);
		timer_chain->tim_active= 0;
		timer_index= timer_chain;
		timer_chain= timer_chain->tim_next;
		(*timer_index->tim_func)(timer_index->tim_ref, timer_index);
	}
	set_timer();
}

PUBLIC void clck_timer(timer, timeout, func, fd)
timer_t *timer;
time_t timeout;
timer_func_t func;
int fd;
{
	timer_t *timer_index;

	if (timer->tim_active)
		clck_fast_release(timer);
	assert(!timer->tim_active);

	timer->tim_next= 0;
	timer->tim_func= func;
	timer->tim_ref= fd;
	timer->tim_time= timeout;
	timer->tim_active= 1;

	if (!timer_chain)
		timer_chain= timer;
	else if (timeout < timer_chain->tim_time)
	{
		timer->tim_next= timer_chain;
		timer_chain= timer;
	}
	else
	{
		timer_index= timer_chain;
		while (timer_index->tim_next &&
			timer_index->tim_next->tim_time < timeout)
			timer_index= timer_index->tim_next;
		timer->tim_next= timer_index->tim_next;
		timer_index->tim_next= timer;
	}
	set_timer();
}

PRIVATE void clck_fast_release (timer)
timer_t *timer;
{
	timer_t *timer_index;

	if (!timer->tim_active)
		return;

	if (timer == timer_chain)
		timer_chain= timer_chain->tim_next;
	else
	{
		timer_index= timer_chain;
		while (timer_index && timer_index->tim_next != timer)
			timer_index= timer_index->tim_next;
		assert(timer_index);
		timer_index->tim_next= timer->tim_next;
	}
	timer->tim_active= 0;
}

