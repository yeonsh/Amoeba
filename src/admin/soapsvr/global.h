/*	@(#)global.h	1.3	96/02/27 10:22:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef GLOBAL_H
#define GLOBAL_H

#include "_ARGS.h"

extern struct sp_table *_sp_table;	/* the directory table proper */
extern int _sp_entries;			/* max number of entries */

#define alloc(s, n) sp_alloc((unsigned)(s), (unsigned)(n))

#define NBULLETS	2	/* only value guaranteed to work, currently */

#define bad_objnum(obj)	((obj) < 1 || (obj) >= _sp_entries)


/*
 * h_offset in a header is used for several purposes:
 */
#define h_seq	h_offset
#define h_mask	h_offset

/*
 * hdr.h_status is set to SP_REPEAT to signal that we failed to get the
 * intention across (the other side was busy at the time) so we should
 * try again.
 */
#define SP_REPEAT	1

#define DEFAULT_TIMEOUT ((interval)2000) /* transaction timeout */

#endif /* GLOBAL_H */
