/*	@(#)event.c	1.1	96/02/27 14:14:37 */
/*
inet/amoeba_dep/event.c

Created:	April 1995 by Philip Homburg <philip@cs.vu.nl>

Implementation of an event queue.

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "generic/assert.h"
#include "generic/event.h"

#include "generic/clock.h"

INIT_PANIC();

event_t *ev_head;
static event_t *ev_tail;
static timer_t ev_timer;

static void process ARGS(( int arg, timer_t *timer ));

void ev_init(ev)
event_t *ev;
{
	ev->ev_func= 0;
	ev->ev_next= NULL;
}

void ev_enqueue(ev, func, ev_arg)
event_t *ev;
ev_func_t func;
ev_arg_t ev_arg;
{
	assert(ev->ev_func == 0);
	ev->ev_func= func;
	ev->ev_arg= ev_arg;
	ev->ev_next= NULL;
	if (ev_head == NULL)
	{
		ev_head= ev;
		clck_timer(&ev_timer, 1, process, 0);
	}
	else
		ev_tail->ev_next= ev;
	ev_tail= ev;
}

static void process(arg, timer)
int arg;
timer_t *timer;
{
	ev_process();
}

void ev_process()
{
	ev_func_t func;
	event_t *curr;

	while (ev_head)
	{
		curr= ev_head;
		ev_head= curr->ev_next;
		func= curr->ev_func;
		curr->ev_func= 0;

		assert(func != 0);
		func(curr, curr->ev_arg);
	}
}

int ev_in_queue(ev)
event_t *ev;
{
	return ev->ev_func != 0;
}

