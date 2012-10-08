/*	@(#)locking.h	1.4	96/03/15 14:07:23 */
/*
 * Copyright 1996 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * LOCK, UNLOCK, and RETURN primitives used in stdio.
 * They are needed to make the use of stdio by multiple threads possible.
 */

#include <amoeba.h>
#include <module/mutex.h>
#include <module/signals.h>

#ifndef NO_SIG_MU_LOCK

#define LOCK(stream, local_count) {				\
	sig_mu_lock(&stream->_mu);	/* Lock this stream */	\
	local_count = stream->_count;   /* Need local copy */	\
	stream->_count = 0;					\
}

#define UNLOCK(stream, local_count) {				 \
	stream->_count = local_count;	/* Publish new _count */ \
        sig_mu_unlock(&stream->_mu);	/* Unlock the stream */	 \
}

#else

#define LOCK(stream, local_count) {				\
	mu_lock(&stream->_mu);		/* Lock this stream */	\
	local_count = stream->_count;   /* Need local copy */	\
	stream->_count = 0;					\
}

#define UNLOCK(stream, local_count) {				 \
	stream->_count = local_count;	/* Publish new _count */ \
        mu_unlock(&stream->_mu);	/* Unlock the stream */	 \
}

#endif

/*
 * RETURN assigns value to variable "result" and jumps to "done",
 * where appropriate terminating actions (such as unlocking and returning)
 * will be performed.
 */

#define RETURN(val)	{ result = (val); goto done; }

