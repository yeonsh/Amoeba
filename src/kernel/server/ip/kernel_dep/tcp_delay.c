/*	@(#)tcp_delay.c	1.1	91/11/25 13:10:17 */
/*
tcp_delay.c

Created Sept 16, 1991 by Philip Homburg
*/

#include "nw_task.h"
#include "amoeba_incl.h"

int tcp_delay_on= 0;
u32_t tcp_delay= 1024;

int tcp_delay_up _ARGS(( char *bp, char *be ));
int tcp_delay_down _ARGS(( char *bp, char *be ));
int tcp_delay_enable _ARGS(( char *bp, char *be ));

int tcp_delay_up(bp, be)
char *bp;
char *be;
{
	char *p;

	p= bp;
	if (!tcp_delay_on)
	{
		p= bprintf(p, be, "tcp_delay is not enabled!");
	}
	else
	{
		tcp_delay *= 2;
		if (!tcp_delay)
			tcp_delay++;
		p= bprintf(p, be, "tcp_delay is now %d ms", tcp_delay);
	}
	if (!p)
		return be-bp;
	else
		return p-bp;
}

tcp_delay_down(bp, be)
char *bp;
char *be;
{
	char *p;

	p= bp;
	if (!tcp_delay_on)
	{
		p= bprintf(p, be, "tcp_delay is not enabled!");
	}
	else
	{
		tcp_delay /= 2;
		if (!tcp_delay)
			tcp_delay++;
		p= bprintf(p, be, "tcp_delay is now %d ms", tcp_delay);
	}
	if (!p)
		return be-bp;
	else
		return p-bp;
}

tcp_delay_enable(bp, be)
char *bp;
char *be;
{
	char *p;

	p= bp;

	tcp_delay_on= !tcp_delay_on;
	if (tcp_delay_on)
	{
		p= bprintf(p, be, "tcp_delay is enabled");
	}
	else
	{
		p= bprintf(p, be, "tcp_delay is disabled");
	}
	if (!p)
		return be-bp;
	else
		return p-bp;
}
