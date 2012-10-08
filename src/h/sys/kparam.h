/*	@(#)kparam.h	1.1	96/02/27 10:39:00 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Structure defining kernel parameters, settable per machine
 */

struct kparam {
	port	kp_port;
	port	kp_chkf;
};
