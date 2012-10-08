/*	@(#)group.h	1.5	96/02/27 10:25:48 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef _GROUP_H_
#define _GROUP_H_

typedef header 		*header_p;
typedef uint32		g_seqcnt_t;
typedef uint16		g_index_t; /* for internal use */
typedef uint32		g_indx_t;  /* for user interface */
typedef uint16  	g_incarno_t;
typedef uint32  	g_msgcnt_t;
typedef int32		g_id_t;


typedef struct {
	g_seqcnt_t 	st_expect;	/* next sequence no to be delivered */
	g_seqcnt_t 	st_nextseqno;	/* next sequence no to be received */
	g_seqcnt_t	st_unstable;	/* next sequence no to be acked */
	g_index_t 	st_total;	/* total number of members */
	g_index_t	st_myid;	/* my member id */
	g_index_t	st_seqid;	/* sequencer id */
} grpstate_t, *grpstate_p;

#define	grp_create	_grp_create
#define	grp_forward	_grp_forward
#define	grp_info	_grp_info
#define	grp_join	_grp_join
#define	grp_leave	_grp_leave
#define	grp_receive	_grp_receive
#define	grp_reset	_grp_reset
#define	grp_send	_grp_send
#define	grp_set		_grp_set

g_id_t   grp_create  _ARGS((port *p, g_indx_t resilience, g_indx_t maxgroup,
			    uint32 nbuf, uint32 maxmess));
errstat  grp_forward _ARGS((g_id_t gid, port *p, g_indx_t memid));
errstat  grp_info    _ARGS((g_id_t gid, port *p, grpstate_p state,
			    g_indx_t *memlist, g_indx_t size));
g_id_t   grp_join    _ARGS((header_p hdr));
errstat  grp_leave   _ARGS((g_id_t gid, header_p hdr));
int32    grp_receive _ARGS((g_id_t gid, header_p hdr,
			    bufptr buf, uint32 cnt, int *more));
g_indx_t grp_reset   _ARGS((g_id_t gid, header_p hdr, g_indx_t n));
errstat  grp_send    _ARGS((g_id_t gid, header_p hdr, bufptr buf, uint32 cnt));
errstat  grp_set     _ARGS((g_id_t gid, port *p, interval sync,
			    interval reply, interval alive, uint32 large));

#endif /* _GROUP_H_ */
