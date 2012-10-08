/*	@(#)sp_buf.h	1.3	96/02/27 11:17:42 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef _SP_BUF_H
#define _SP_BUF_H

#include "_ARGS.h"

union sp_buf {
    char          sp_buffer[SP_BUFSIZE];
    union sp_buf *sp_buf_next;
};

#define	sp_getbuf	_sp_getbuf
#define	sp_putbuf	_sp_putbuf

union sp_buf *sp_getbuf _ARGS((void));
void	      sp_putbuf _ARGS((union sp_buf *buf));

#endif
