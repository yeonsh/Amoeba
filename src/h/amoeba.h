/*	@(#)amoeba.h	1.5	96/02/27 10:24:35 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __AMOEBA_H__
#define __AMOEBA_H__

/* Macros which give what the sizes of the network structures SHOULD be */
#define	PORTSIZE	 6
#define	OOBSIZE		20
#define	HEADERSIZE	32
#define	PRIVSIZE	10
#define	CAPSIZE		16

#include <am_types.h>

/* For transaction interface routines */
typedef	char *		bufptr;		/* pointer to transaction buffer */
typedef	uint16		bufsize;	/* transaction buffer size */

/* For sleep and wakeup and semaphores */
typedef	char *		event;
typedef long		mutex;


/* Used in headers and capabilities */
typedef	int32		rights_bits;	/* rights bits in a capability */
#define buf_put_right_bits		buf_put_int32
#define buf_get_right_bits		buf_get_int32

typedef	int32		objnum;		/* object number in a capability */
#define	buf_put_objnum			buf_put_int32
#define	buf_get_objnum			buf_get_int32

/* For now the capability only holds 24 rights bits */
#define	MAX_OBJNUM	((1 << 24) - 1)

typedef	uint16		command;	/* command as in header */
typedef int32		errstat;	/* error status as used by stubs */

#define	ERR_STATUS(n)	((short) (n) < 0)
#define	ERR_CONVERT(n)	((short)(n))

typedef	int32		interval;	/* timeout in milliseconds */


/*
 * There are currently two possible hacks for comparing ports.
 * The first is to compare a long and a short (instead of 6 bytes).
 * This only works for a limited number of machines.  Add the preprocessor
 * define for FAKEPORT_HACK_1 below if your machine can do it.
 * (The sparc chip, for example, cannot do it.)
 */

#ifdef mc68000
#	define	FAKEPORT_HACK_1
#endif

#ifdef vax
#	define	FAKEPORT_HACK_1
#endif


struct _fakeport {
#ifdef FAKEPORT_HACK_1
	long		_p1;
	short		_p2;
#else /* FAKEPORT_HACK_1 */
	short		_p1,_p2,_p3;
#endif /* FAKEPORT_HACK_1 */
};


typedef	struct {
	int8		_portbytes[PORTSIZE];
} port;


typedef	struct {
	int8		prv_object[3];	/* becomes an objnum in amoeba 4 */
	uint8		prv_rights;	/* becomes a rights_bits in amoeba 4 */
	port		prv_random;
} private;


typedef	struct {
	port		cap_port;
	private		cap_priv;
} capability;


typedef	struct {
	port		h_port;
	port		h_signature;
	private		h_priv;
	command		h_command;
	int32		h_offset;
	bufsize		h_size;
	uint16		h_extra;
} header;

#define	h_status	h_command	/* alias: reply status */


#define	SIZEOFTABLE(t)	(sizeof(t) / sizeof((t)[0]))

#define	NILPORT		((port *) 0)
#define	NILBUF		((bufptr) 0)


/* The following hack is to optimise comparisons of ports */
#define	_FP(p)		((struct _fakeport *) (p))

#ifndef lint
#ifdef FAKEPORT_HACK_1
#	define	PORTCMP(p, q)	(_FP(p)->_p1==_FP(q)->_p1 && \
				 _FP(p)->_p2==_FP(q)->_p2)
#	define	NULLPORT(p)	(_FP(p)->_p1==0 && _FP(p)->_p2==0)
#else /* FAKEPORT_HACK_1 */
#	define	PORTCMP(p, q)	(_FP(p)->_p1==_FP(q)->_p1 && \
				 _FP(p)->_p2==_FP(q)->_p2 && \
				 _FP(p)->_p3==_FP(q)->_p3)
#	define	NULLPORT(p)	(_FP(p)->_p1==0 && _FP(p)->_p2==0 && \
				 _FP(p)->_p3==0)
#endif /* FAKEPORT_HACK_1 */
#else /* lint */
#	define	PORTCMP(p, q)	((p)->_portbytes[0] == (q)->_portbytes[0])
#	define	NULLPORT(p)	((p)->_portbytes[0] == 0)
#endif /* lint */

#define	trans		_trans
#define	getreq		_getreq
#define	putrep		_putrep
#define	timeout		_timeout
#define	priv2pub	_priv2pub
#define	getcap		_getcap

#include <_ARGS.h>

#ifndef ail

typedef int	_bufsize; /* syscall stubs expect 4 bytes as function arg */
/*
** Some function declarations that people tend to forget.
*/
bufsize	 trans _ARGS((header *, bufptr, _bufsize, header *, bufptr, _bufsize));
bufsize	 getreq _ARGS((header *, bufptr, _bufsize));
void	 putrep _ARGS((header *, bufptr, _bufsize));
interval timeout _ARGS((interval));
void	 priv2pub _ARGS((port *, port *));
capability * getcap _ARGS(( char * ));
#endif

#endif /* __AMOEBA_H__ */
