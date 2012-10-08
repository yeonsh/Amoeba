/*	@(#)fifo_impl.h	1.3	94/04/06 11:44:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef FIFO_IMPL_H
#define FIFO_IMPL_H

#include <_ARGS.h>

/* Implementation routines of the operators: */
errstat impl_fifo_create  _ARGS((header *h, objnum obj, capability *fifo_cap));
errstat impl_fifo_open    _ARGS((header *h, objnum obj, int flags,
			         capability *fifo_handle));
errstat impl_fifo_share   _ARGS((header *h, objnum obj));
errstat impl_fifo_close   _ARGS((header *h, objnum obj));
errstat impl_fifo_bufsize _ARGS((header *h, objnum obj, long int *size));

errstat impl_fsq_read     _ARGS((header *h, objnum obj, long int position, 
				 int size,  char *data, int max_data,
				 int *more, int *res));
errstat impl_fsq_write    _ARGS((header *h, objnum obj, long int position, 
				 int size,  char *data, int max_data,
				 int *written));

struct fifo_t;
struct fp;

int	fifo_stat         _ARGS((struct fifo_t *f, char *buf, int size));
int	fifo_global_stat  _ARGS((char *buf, int size));

void	fifo_lock	  _ARGS((struct fifo_t *f));
void	fifo_unlock	  _ARGS((struct fifo_t *f));

errstat del_fifo	  _ARGS((struct fifo_t *fifo));
void    init_fifo	  _ARGS((int numfifo, port checkfields[]));

struct fifo_t *fifo_entry _ARGS((objnum obj));
struct fp     *fp_entry   _ARGS((objnum obj));

#endif /* FIFO_IMPL_H */
