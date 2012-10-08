/*	@(#)capset.h	1.4	96/02/27 10:25:06 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __CAPSET_H__
#define __CAPSET_H__

#define MAXCAPSET	8

typedef struct {
	capability s_object;	/* object capability */
	short s_current;	/* current or not */
} suite;

typedef struct {
	short cs_initial;	/* initial number of copies */
	short cs_final;		/* final number of copies */
	suite *cs_suite;	/* capability suite */
} capset;

#define	cs_copy		_cs_copy
#define	cs_free		_cs_free
#define	cs_member	_cs_member
#define	cs_goodcap	_cs_goodcap
#define	cs_to_cap	_cs_to_cap
#define	cs_purge	_cs_purge
#define	cs_singleton	_cs_singleton

#define	buf_get_capset	_buf_get_capset
#define	buf_put_capset	_buf_put_capset

int	cs_copy _ARGS((capset *, capset *));
void	cs_free _ARGS((capset *));
int	cs_member _ARGS((capset *, capability *));
errstat	cs_goodcap _ARGS((capset *, capability *));
errstat	cs_to_cap _ARGS((capset *, capability *));
errstat	cs_purge _ARGS((capset *));
int	cs_singleton _ARGS((capset *, capability *));

char *	buf_get_capset	_ARGS((char *p, char *endbuf, capset *cs));
char *	buf_put_capset	_ARGS((char *p, char *endbuf, capset *cs));

#endif /* __CAPSET_H__ */
