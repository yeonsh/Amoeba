/*	@(#)event.h	1.1	96/02/27 14:07:12 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* A module for doing sleep/wakeup on events */
#include <amoeba.h>
#include "semaphore.h"
#include <module/mutex.h>

typedef struct {
	semaphore	guard ;
	semaphore	sleeper ;
	int		count ;
} b_event ;

void	event_init _ARGS((b_event *)) ;
void	event_incr _ARGS((b_event *)) ;
void	event_wait _ARGS((b_event *)) ;
int	event_trywait _ARGS((b_event *,interval)) ;
void	event_wake _ARGS((b_event *)) ;
void	event_cancel _ARGS((b_event *)) ;

int	get_event _ARGS((b_event **)) ;
b_event *map_event _ARGS((int)) ;
errstat free_event _ARGS((int)) ;

void	typed_lock _ARGS((int lclass, long number)) ;
int	typed_trylock _ARGS((int lclass, long number)) ;
int	test_typed_lock _ARGS((int lclass, long number)) ;
void	typed_unlock _ARGS((int lclass, long number)) ;
void	typed_lock_init _ARGS((void)) ;

/* Lock classes, for typed...lock... */
#define	INODE_LCLASS	0
#define BLOCK_LCLASS	1
