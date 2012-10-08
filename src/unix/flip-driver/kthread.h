/*	@(#)kthread.h	1.3	94/04/07 14:02:27 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __KTHREAD_H__
#define __KTHREAD_H__

typedef struct rpc_device thread_t;

extern thread_t *thread, *curthread;

#endif /* __KTHREAD_H__ */
