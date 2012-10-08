/*	@(#)timer.h	1.3	94/04/06 11:59:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef TIMER_H
#define TIMER_H

void time_update   _ARGS((void));
long sp_time	   _ARGS((void));

#ifdef SOAP_GROUP
long sp_time_start _ARGS((void));
long sp_mytime	   _ARGS((void));
#endif

#endif /* TIMER_H */
