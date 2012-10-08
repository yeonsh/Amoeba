/*	@(#)saksvr.h	1.3	96/02/27 10:38:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SVR_SAKSVR_H__
#define __SVR_SAKSVR_H__

#include	"amtools.h"
#include	<stdlib.h>
#include	"ampolicy.h"
#include	<sys/types.h>
#include	"bullet/bullet.h"
#include	<time.h>
#include	"thread.h"
#include	"monitor.h"
#include	"sakconf.h"
#include	"sakfunc.h"
#include	"sak.h"

#define	FALSE		0
#define	TRUE		1

long atol ();
#define	ato_objnum	(objnum) atol

#define	STRN_EQUAL(a, b, len)		(strncmp (a, b, len) == 0)
#define	STRN_NOT_EQUAL(a, b, len)	(strncmp (a, b, len) != 0)

struct sak_job {
	objnum		ident;			/* identity nummer of job */
	port		rnd;			/* random part of capability
						 * of job */
	int8		age;			/* age (when cap of job not
						 * saved in a directory, it
						 * will be deleted in 7 days)
						 */
	struct sak_job_options opt;

	int8		* schedule[MAX_SPEC];
	struct sak_job	*next;			/* pointer to next job in
						 * list
						 */

	int8		delayed;
	time_t		executiondate;		/* next execution date */
	struct sak_job	*time_next;		/* pointer to next job in
						 * runlist list
						 */
};

struct trans_info {
	header hdr;
	bufsize req_size, reply_size;
	interval timeout;
	char *buf;
};

#define	TRANS_INFO_BUF_SIZE	(sizeof (struct trans_info) * 2)


extern struct sak_job *joblist[];	
extern mutex job_list_sem;
extern capability supercap;
extern port superrnd;

#define	hashval(x)	((int) ((long) x % MODVAL))
#define newcell()	((struct sak_job *) malloc (sizeof (struct sak_job)))
#define	NILCELL		(struct sak_job *) 0

#define	OWNERRIGHTS	((rights_bits) 0xff)
#define	GROUPRIGHTS	((rights_bits) 0x00)
#define	OTHERRIGHTS	((rights_bits) 0x00)
#define	Bitset(x, y)	((x & y) == y)

#define	FREE		0
#define	WORK_TYPE	1
#define	REQ_TYPE	2
#define	AGE_TYPE	3
#define	EXEC_TYPE	4

#ifdef DEBUG
extern int debug;
#endif

#endif /* __SVR_SAKSVR_H__ */
