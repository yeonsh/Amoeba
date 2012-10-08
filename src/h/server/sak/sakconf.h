/*	@(#)sakconf.h	1.2	94/04/06 17:05:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SVR_SAKCONF_H__
#define __SVR_SAKCONF_H__

#define	NR_OF_THREADS_DEFAULT	10	/* nr of threads in pool */
#define	NR_OF_REQ_THREADS	2	/* nr of threads to handle requests */
#define	NR_OF_EXECTHREADS	2	/* nr of threads to handle exec
					 * requests
					 */

#define	POOL_STACK	(16 * 1024) /* stack size used for threads */
				

#define	ALLRIGHTS	((rights_bits) 0xff)
#define	MODVAL		61		/* used as hash value */
#define	MAXAGE		7*24		/* 7 days */
#define	GENERATIC	((objnum) 0)

#define	JOBPREFIX	"job"
#define	JOBPREFIX_LEN	3

#define	TRANSPREFIX	"trans"
#define	TRANSPREFIX_LEN	5

#define	JOBDIR		"jobs"
#define	TRANSDIR	"trans"
#define	FNAME_LEN	32

#define	SEC			(interval) 1000
#define	NORMAL_TIMEOUT		((interval) 10 * SEC)
#define	SHORT_TIMEOUT		((interval) 5 * SEC)

/* MAX_TIMEOUT is maximum time for a job transaction
 * better keep MAX_TIMEOUT low otherwise it could keep to many threads
 * waiting  and the server might run out of threads
 * (which is still possible by a malicious user, by scheduling a job to
 * be executed every minute with a specified transcation that will
 * hang, eventualy server will run out of threads)
 */
#define	MAX_TIMEOUT		((interval) 30 * SEC)

/* MAXIMUM time the server will sleep , keep it to one hour or less
 * to let it notice changes in system time in a reasonable interval
 */
#define	MAX_SLEEP		(60 * 60)	/* 1 hour */

#define	MFBUFSIZE		1024	/* used for masterfile size */
#define	JFBUFSIZE		1024	/* used for job file size */

#define	TRANSBUFSIZE		1024	/* used for server
					 * transaction bufsize
					 */
#endif /* __SVR_SAKCONF_H__ */
