/*	@(#)amparam.h	1.4	96/02/27 11:54:08 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __AMPARAM_H__
#define __AMPARAM_H__

#ifdef UNIX

typedef struct {
	struct param {
		header	*par_hdr;
		bufptr	par_buf;
		uint16	par_cnt;
	} tp_par[2];
	uint16	tp_maxloc;
} trpar;

#if defined(BSD42) || defined(__SVR4)

#ifdef __STDC__
/* use the _IO/_IOW definitions as fixed for ANSI C */

/* ACK ANSI C doesn't allow pointers and longs to be two byte aligned.
 * The amoeba driver does, however, expect the trpar argument in this
 * format. So, just before the ioctl call we `compact' the trpar tructure.
 * This means that sizeof(trpar) and sizeof(struct param) isn't right
 * in our case. Therefore we introduce the following 2 unaligned types, that
 * let the _IO/_IOW macros expand correctly.
 */
#define PARAM_SIZE	(sizeof(header *) + sizeof(bufptr) + sizeof(uint16))
typedef char unaligned_param [PARAM_SIZE];
typedef char unaligned_trpar [2*PARAM_SIZE+sizeof(uint16)];
		
#define AM_GETREQ	_IOW('a',1,unaligned_param)
#define AM_PUTREP	_IOW('a',2,unaligned_param)
#define AM_TRANS	_IOW('a',3,unaligned_trpar)
#define AM_DUMP		_IO('a',4)
#define AM_TIMEOUT	_IO('a',5)		/* not used */
#define AM_CLEANUP	_IO('a',6)

#else

#define AM_GETREQ	_IOW(a,1,struct param)
#define AM_PUTREP	_IOW(a,2,struct param)
#define AM_TRANS	_IOW(a,3,trpar)
#define AM_DUMP		_IO(a,4)
#define AM_TIMEOUT	_IO(a,5)		/* not used */
#define AM_CLEANUP	_IO(a,6)

#endif /*__STDC__*/

#else /*defined(BSD42) || defined(__SVR4)*/

#define AM_GETREQ	('a' << 8 | 1)
#define AM_PUTREP	('a' << 8 | 2)
#define AM_TRANS	('a' << 8 | 3)
#define AM_DUMP		('a' << 8 | 4)
#define AM_TIMEOUT	('a' << 8 | 5)		/* not used */
#define AM_CLEANUP	('a' << 8 | 6)

#endif /*defined(BSD42) || defined(__SVR4)*/

#endif /*UNIX*/

#endif /* __AMPARAM_H__ */
