/*	@(#)sigsvr.h	1.2	94/04/06 15:47:56 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __SIGSVR_H__
#define __SIGSVR_H__

/* This file defines the interface between the session server and a process
   to send signals. */

#define AJAX_SENDSIG 10100
	/* The process should send itself a signal.
	   In the request, h_extra specifies the signal number.
	   In the reply, h_offset is nonzero if the signal was handled. */

#endif /* __SIGSVR_H__ */
