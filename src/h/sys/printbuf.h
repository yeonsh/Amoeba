/*	@(#)printbuf.h	1.3	94/05/17 10:29:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __PRINTBUF_H__
#define __PRINTBUF_H__

/*
**	printbuf - the in core copy of the console is managed using the
**		   following struct and define.
*/

/* header which keeps track of current start of circular buffer */
struct printbuf {
	char *pb_next;
	char *pb_first;
	char *pb_buf;
	char *pb_end;
};

extern struct printbuf *pb_descr;

#endif /* __PRINTBUF_H__ */
