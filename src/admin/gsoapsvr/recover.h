/*	@(#)recover.h	1.1	96/02/27 10:02:55 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#ifndef RECOVER_H
#define RECOVER_H

errstat rec_get_state     _ARGS((int helper));
errstat rec_send_updates  _ARGS((header *inhdr, char *inbuf, int insize));
int     rec_recovered	  _ARGS((objnum obj));
void    rec_await_updates _ARGS((void));

void    highest_dir_seqno _ARGS((sp_seqno_t *seqno));

#endif
