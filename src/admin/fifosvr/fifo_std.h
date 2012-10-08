/*	@(#)fifo_std.h	1.3	94/04/06 11:45:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef IMPL_STD_H
#define IMPL_STD_H

errstat impl_std_restrict _ARGS((header *h, objnum obj, long mask,
				 capability *newcap));
errstat impl_std_info     _ARGS((header *h, objnum obj, char *reply,
				 int max_reply, int size, int *retlen));
errstat impl_std_status   _ARGS((header *h, objnum obj, char *reply,
				 int max_reply, int size, int *retlen));
errstat impl_std_touch    _ARGS((header *h, objnum obj));
errstat impl_std_destroy  _ARGS((header *h, objnum obj));

void init_std _ARGS((void));

#endif /* IMPL_STD_H */
