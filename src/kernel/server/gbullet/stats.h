/*	@(#)stats.h	1.1	96/02/27 14:07:39 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __STATS_H__
#define __STATS_H__

/*
 *	STATS.H
 *
 *	The statistic struct holds the record of statistics for the bullet
 *	server.  The statistics reflect only the state of the system since
 *	it was last (re)started.
 *	Each field is a count of the number of times the particular command
 *	type has been executed.
 */

typedef	struct Statistics	Statistics;

struct Statistics
{
    /* Standard Commands */
    long	std_age;	/* # requests to age and garbage collect */
    long	std_localcopy;	/* # requests to replicate a local file */
    long	std_remotecopy;	/* # requests to replicate a remote file */
    long	std_destroy;	/* # files deleted */
    long	std_info;	/* # std_info requests */
    long	std_restrict;	/* # std_restrict requests */
    long	std_touch;	/* # std_touch requests */
    long	std_setparams;	/* # std_setparams requests */
    long	std_getparams;	/* # std_getparams request */
    /* Bullet Commands */
    long	b_create;	/* # files created */
    long	b_read;		/* # read requests */
    long	b_modify;	/* # modify edit operations */
    long	b_insert;	/* # insert edit operations */
    long	b_delete;	/* # delete edit operations */
    long	b_size;		/* # requests for file size */
    long	b_fsck;		/* # requests to check file system */
    long	b_status;	/* # bullet server status requests */
    long	b_sync;		/* # sync commands executed */
    long	b_log;		/* # log commands executed */
    long	b_disk_comp;	/* # disk compaction requests */
    long	b_dcfmoved;	/* # files moved during disk compaction */
    long	b_replicate;	/* number of replications */
    /* Internal Storage Server Matters */
    long	i_lastsweep;	/* # Commands since the previous sweep */
    long	i_cache_hit;	/* # reads,edits,etc, which got a cache hit */
    long	i_cache_miss;	/* and those that didn't */
    long	i_compact;	/* # cache compactions */
    long	i_readdata;	/* # times b_read actually went to disk */
    long	i_disk_read;	/* # calls to read the disk */
    long	i_disk_write;	/* # calls to write the disk */
    long	i_aged_files;	/* # files destroyed by aging */
    long	i_uncomm_tout;	/* # uncommitted files that timed out */
    /* Group matters */
    long	g_fsck;		/* # requests to check file system */
    long	g_status;	/* # bullet server status requests */
    long	g_sync;		/* # sync commands executed */
    long	g_disk_comp;	/* # disk compaction requests */
    long	g_repchk;	/* # replication checks */
    long	g_attach;	/* # new member requests */
    long	g_state;	/* # state requests */
};

#endif /* __STATS_H__ */
