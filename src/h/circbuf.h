/*	@(#)circbuf.h	1.3	96/02/27 10:25:18 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __CIRCBUF_H__
#define __CIRCBUF_H__

/*
** Circular buffer structure.
*/
#include "_ARGS.h"

/*
** (struct circbuf is revealed in circbuf.c)
*/

#define	cb_alloc	_cb_alloc
#define	cb_free		_cb_free
#define	cb_close	_cb_close
#define	cb_setsema	_cb_setsema
#define	cb_full		_cb_full
#define	cb_empty	_cb_empty
#define	cb_putc		_cb_putc
#define	cb_getc		_cb_getc
#define	cb_trygetc	_cb_trygetc
#define	cb_puts		_cb_puts
#define	cb_gets		_cb_gets
#define	cb_getp		_cb_getp
#define	cb_getpdone	_cb_getpdone
#define	cb_putp		_cb_putp
#define	cb_putpdone	_cb_putpdone

struct circbuf *cb_alloc _ARGS((int size));
void cb_free _ARGS((struct circbuf *cb));
void cb_close _ARGS((struct circbuf *cb));
void cb_setsema _ARGS((struct circbuf *cb, semaphore *sema));

int cb_full _ARGS((struct circbuf *cb));
int cb_empty _ARGS((struct circbuf *cb));

int  cb_putc _ARGS((struct circbuf *cb, int c));
int  cb_getc _ARGS((struct circbuf *cb));
int  cb_trygetc _ARGS((struct circbuf *cb, int wait));

int  cb_puts _ARGS((struct circbuf *cb, char *s, int len));
int  cb_gets _ARGS((struct circbuf *cb, char *s, int minlen, int maxlen));
int  cb_getp _ARGS((struct circbuf *cb, char **s, int wait));
void cb_getpdone _ARGS((struct circbuf *cb, int len));
int  cb_putp _ARGS((struct circbuf *cb, char **s, int wait));
void cb_putpdone _ARGS((struct circbuf *cb, int len));

#endif /* __CIRCBUF_H__ */
