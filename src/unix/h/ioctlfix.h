/*	@(#)ioctlfix.h	1.3	96/02/27 11:54:50 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __IOCTL_FIX_H__
#define __IOCTL_FIX_H__

#if defined(__STDC__) && !defined(__SVR4)  /* ie. Solaris 2 */

/* Replace the definition of _IOWR from <sys/ioccom.h> by one that works
 * for ANSI C.
 */
#undef  _IOWR
#define _IOWR(x,y,t)    (_IOC_INOUT|((sizeof(t)&_IOCPARM_MASK)<<16)|(x<<8)|y)
#endif

#endif
