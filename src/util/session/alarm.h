/*	@(#)alarm.h	1.2	94/04/07 16:02:33 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <_ARGS.h>
#include <sys/types.h>

/* alarm.c */
int set_timer _ARGS((pid_t pid , unsigned int nsec ));
void start_alrmsvr _ARGS((void ));

