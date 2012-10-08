/*	@(#)queue.c	1.5	96/02/27 14:21:42 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "tty/term.h"
#include "assert.h"
INIT_ASSERT

void
putc(q, c)
register struct queue *q;
int c;
{
#if !VERY_SIMPLE
  register i;
#endif /* !VERY_SIMPLE */

  compare(q->q_head, !=, 0);
  compare(q->q_tail, !=, 0);
  if (q->q_nchar == QUEUESIZE) {
	printf("queue full\n");
	return;
  }
#if !VERY_SIMPLE
  i = q->q_tail - q->q_data;
  if (c & 0x100)
  	q->q_litt[i >> 4] |= 1 << (i & 0xF);
  else
  	q->q_litt[i >> 4] &= ~(1 << (i & 0xF));
#endif /* !VERY_SIMPLE */
  *q->q_tail++ = c;
  if (q->q_tail == &q->q_data[QUEUESIZE])
	q->q_tail = &q->q_data[0];
  q->q_nchar++;
}

getc(q)
register struct queue *q;
{
  register c;
#if !VERY_SIMPLE
  register i;
#endif /* !VERY_SIMPLE */

  compare(q->q_head, !=, 0);
  compare(q->q_tail, !=, 0);
  if (q->q_nchar == 0)
	return -1;
#if !VERY_SIMPLE
  i = q->q_head - q->q_data;
#endif /* !VERY_SIMPLE */
  c = *q->q_head++ & 0xFF;
  q->q_nchar--;
  if (q->q_head == &q->q_data[QUEUESIZE])
	q->q_head = &q->q_data[0];
#if !VERY_SIMPLE
  if (q->q_litt[i >> 4] & 1 << (i & 0xF))
	c |= 0x100;
#endif /* !VERY_SIMPLE */
  return c;
}

#if !VERY_SIMPLE
lastc(q)
register struct queue *q;
{
  register c;
  register i;

  if (q->q_nchar == 0)
	return -1;
  i = q->q_tail - q->q_data - 1;
  if (i < 0)
	i += QUEUESIZE;
  c = q->q_data[i];
  if (q->q_litt[i >> 4] & 1 << (i & 0xF))
	c |= 0x100;
  return c;
}

unputc(q)
register struct queue *q;
{
  register c;
  register i;

  if (q->q_nchar == 0)
	return -1;
  if (q->q_tail == q->q_data)
	q->q_tail = &q->q_data[QUEUESIZE - 1];
  else
	q->q_tail--;
  q->q_nchar--;
  c = *q->q_tail;
  i = q->q_tail - q->q_data;
  if (q->q_litt[i >> 4] & 1 << (i & 0xF))
	c |= 0x100;
  return c;
}

nextc(q)
register struct queue *q;
{
  register c;
  register i;

  compare(q->q_head, !=, 0);
  compare(q->q_tail, !=, 0);
  if (q->q_nchar == 0)
	return -1;
  if (q->q_extra == 0)
	q->q_extra = q->q_head;
  if (q->q_extra == q->q_tail) {
  	q->q_extra = 0;
	return -1;
  }
  i = q->q_extra - q->q_data;
  c = *q->q_extra++ & 0xFF;
  if (q->q_extra == &q->q_data[QUEUESIZE])
	q->q_extra = &q->q_data[0];
  if (q->q_litt[i >> 4] & 1 << (i & 0xF))
	c |= 0x100;
  return c;
}
#endif /* !VERY_SIMPLE */

#if !VERY_SIMPLE || XONXOFF
insc(q, c)
register struct queue *q;
{
#if !VERY_SIMPLE
  register i;
#endif /* !VERY_SIMPLE */

  compare(q, !=, 0);
  if (q->q_head == q->q_data)
	q->q_head = &q->q_data[QUEUESIZE - 1];
  else
	q->q_head--;
#if !VERY_SIMPLE
  i = q->q_head - q->q_data;
  if (c & 0x100)
	q->q_litt[i >> 4] |= 1 << (i & 0xF);
  else
	q->q_litt[i >> 4] &= ~(1 << (i & 0xF));
#endif /* !VERY_SIMPLE */
  *q->q_head = c;
}
#endif /* !VERY_SIMPLE || XONXOFF */

flushqueue(q)
register struct queue *q;
{
  q->q_head = q->q_tail = &q->q_data[0];
  q->q_nchar = 0;
#if !VERY_SIMPLE
  q->q_extra = 0;
#endif /* !VERY_SIMPLE */
}
