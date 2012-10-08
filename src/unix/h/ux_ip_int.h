/*	@(#)ux_ip_int.h	1.3	94/04/07 14:04:22 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __UX_IP_INT_H__
#define __UX_IP_INT_H__

#ifdef __STDC__
#include "ioctlfix.h"

#define F_IP_WRITE	_IOWR('f', FL_IP_WRITE, struct ip_args)
#define F_IP_READ	_IOWR('f', FL_IP_READ, struct ip_args)
#else
#define F_IP_WRITE	_IOWR(f, FL_IP_WRITE, struct ip_args)
#define F_IP_READ	_IOWR(f, FL_IP_READ, struct ip_args)
#endif

#define MAXIPBUF	1000

struct ip_args {
    int 	ip_status;
    location 	ip_loc;
    int 	ip_dirsize;
    int 	ip_totsize;
    char       *ip_buffer;
};

#endif /* __UX_IP_INT_H__ */
