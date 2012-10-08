/*	@(#)proc.h	1.1	96/02/27 10:35:53 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __PROCESS_PROC_H__
#define __PROCESS_PROC_H__

/* name of the process server directory entry in the processor directory */

#define	PROCESS_SVR_NAME	"proc"

/* name of the process list directory entry likewise */

#define PROCESS_LIST_NAME	"ps"

/*
** Tunable Constant:  This is the maximum size expected for any fault frame
** struct for any machine you expect to run processes on.  The number below
** is big enough for all the machines we've met so far.  Only alter it if
** needs to be bigger.  It only affects memory allocation activities.
*/

#define	MAX_FAULT_FRAME		512	/* Enough for anything upto now */

/* The largest a thread can become through piling everything into it. */
#define	MAX_THREAD_SIZE		(sizeof (thread_d) + \
				 sizeof (thread_kstate) + \
				 MAX_FAULT_FRAME)

/*
** Process server command codes.
*/

#define PS_STUN			PS_FIRST_COM	    /* pro_stun() */
#define PS_GETOWNER		(PS_FIRST_COM + 1)  /* get owner capability */
#define PS_SETOWNER		(PS_FIRST_COM + 2)  /* set owner capability */

#define PS_EXEC			(PS_FIRST_COM + 3)  /* pro_exec() */

#define PS_GETDEF		(PS_FIRST_COM + 4)  /* Get mmu parameters */
#define PS_GETLOAD		(PS_FIRST_COM + 5)  /* Get cpu load params */

/* commands for managing segments */
#define	PS_SEG_CREATE		(PS_FIRST_COM + 6)  /* create a segment */
#define	PS_SEG_WRITE		(PS_FIRST_COM + 7)  /* write a segment */

/*
** Process owner command codes (from kernel to owner).
*/
#define PS_CHECKPOINT		(PS_FIRST_COM + 8)  /* process checkpoint */

/* Commands to set/get a comment (usually the command line string) */
#define PS_SETCOMMENT		(PS_FIRST_COM + 9)
#define PS_GETCOMMENT		(PS_FIRST_COM + 10)

/* Command to tell an owner its property has a new process capability */
#define PS_SWAPPROC		(PS_FIRST_COM + 11)

/* New getload command for secure process server */
#define	PS_SGETLOAD		(PS_FIRST_COM + 12)

/* Segment identifier returned by seg_map() */
typedef long	segid;

#ifndef ail

/*
** Rights bits in process or segment capabilities.
*/
#define PSR_READ	((rights_bits) 0x01)
#define PSR_WRITE	((rights_bits) 0x02)
#define PSR_CREATE	((rights_bits) 0x04)
#define PSR_DELETE	((rights_bits) 0x08)
#define PSR_EXEC	((rights_bits) 0x10)
#define PSR_KILL	((rights_bits) 0x20)
#define PSR_ALL		((rights_bits) 0xff)

#endif /* ail */

/* Error codes are obtained from stderr.h */

/*
** Segment descriptor.
*/
typedef struct segment_d {
	capability	sd_cap;		/* Capability (if unmapped) */
	long		sd_offset;	/* Offset in file */
	long		sd_addr;	/* Virtual address */
	long		sd_len;		/* Length (bytes) */
	long		sd_type;	/* Type bits (see below) */
} segment_d;

/* Bits in sd_type (also for seg_map) */
#define MAP_GROWMASK	0x0000000f	/* Growth direction */
#define MAP_GROWNOT	0x00000000
#define MAP_GROWUP	0x00000001
#define MAP_GROWDOWN	0x00000002

#define MAP_TYPEMASK	0x000000f0	/* Text/data indication */
#define MAP_TYPETEXT	0x00000010
#define MAP_TYPEDATA	0x00000020

#define MAP_PROTMASK	0x00000f00	/* Read/write indication */
#define MAP_READONLY	0x00000100
#define MAP_READWRITE	0x00000300

#define MAP_SPECIAL	0x0f000000
#define MAP_INPLACE	0x01000000	/* Map in place, don't copy */
#define MAP_AND_DESTROY	0x02000000	/* Map, and std_destroy original */

#define MAP_BINARY	0xf0000000	/* Flags only in binary file */
#define MAP_SYSTEM	0x80000000	/* This seg wil recv args */
#define MAP_SYMTAB	0x40000000	/* Symbol table */
#define MAP_NEW_SYMTAB	0x20000000	/* New style symbol table */

/*
** Thread descriptor.
** This is followed by additional data indicated by td_extra.
** Extra data structures are present in the order of ascending bits.
*/
typedef struct thread_d {
	long		td_state;	/* State */
	long		td_len;		/* Total length */
	long		td_extra;	/* bitvector of extra stuff */
} thread_d;

/* Bits in td_state (some are kernel internals) */
#define TDS_RUN		0x0001	/* Thread is runnable */
#define TDS_DEAD	0x0002	/* (kernel only) thread is busy dying */
#define TDS_EXC		0x0004	/* Thread got some sort of trap */
#define TDS_INT		0x0008	/* (kernel only) thread got interrupt */
#define TDS_FIRST	0x0010	/* Schedule this thread first */
#define TDS_NOSYS	0x0020	/* Thread cannot do syscalls */
#define TDS_START	0x0040	/* (kernel only) immigrating thread */
#define TDS_SRET	0x0080  /* Thread is returning from sig */
#define TDS_LSIG	0x0100	/* Thread got lw signal */
#define TDS_STUN	0x0200	/* Thread stunned */
#define TDS_HSIG	TDS_STUN /* Compatibility */
#define TDS_SHAND	0x0400	/* Thread is calling sighand */
#define TDS_DIE		0x0800	/* DIE! */
#define TDS_STOP	0x1000	/* Stopped for stun handling */
#define TDS_USIG	0x2000	/* Sig should be delivered to user */
#define TDS_CONTINUE	0x4000  /* Continue thread after signal handling */

/* Bits in td_extra, indicating presence of extra data structures: */
#define TDX_IDLE	0x00000001	/* pc/sp struct follows */
#define TDX_KSTATE	0x00000002	/* kstate struct follows */
#define TDX_USTATE	0x00010000	/* ustate struct follows */

/* Corresponding structures */

typedef struct thread_idle {
	long		tdi_pc;
	long		tdi_sp;
} thread_idle;

typedef struct thread_kstate {
	long		tdk_timeout;
	long		tdk_sigvec;
	long		tdk_svlen;
	long		tdk_signal;
	long		tdk_local;
} thread_kstate;

/* thread_ustate is defined in fault.h */

#define	ARCHSIZE	8
/*
** Process descriptor.
** This is followed by pd_nseg segment descriptors (segment_d),
** reachable through PD_SD(p)[i], for 0 <= i < p->pd_nseg.
** The index in the segment array is also the segment identifier.
** Following the segments are pd_nthread variable-lenght thread descriptors.
** Sample code to walk through the threads:
**	thread_d *t = PD_TD(p);
**	for (i = 0; i < p->pd_nthread; ++i, t = TD_NEXT(t))
**		<here *t points to thread number i>;
*/
typedef struct {
	char		pd_magic[ARCHSIZE];	/* Architecture */
	capability	pd_self;	/* Process capability (if running) */
	capability	pd_owner;	/* Default checkpoint recipient */
	uint16		pd_nseg;	/* Number of segments */
	uint16		pd_nthread;	/* Number of threads */
} process_d;

/*
 * The largest size for a process descriptor - including all the thread
 * and segment descriptors.
 */
#define	MAX_PDSIZE	30000

#define PD_SD(p)	((segment_d *) ((p) + 1))
#define PD_TD(p)	((thread_d *) (PD_SD(p) + (p)->pd_nseg))
#define TD_NEXT(t)	((thread_d *) ((char *) (t) + (t)->td_len))

/* possible sizes of a complete thread descriptor */
#define TDSIZE_MIN	(sizeof (thread_d) + \
			 sizeof (thread_kstate) + \
			 USTATE_SIZE_MIN)

#define TDSIZE_MAX	(sizeof (thread_d) + \
			 sizeof (thread_kstate) + \
			 USTATE_SIZE_MAX)

/*
** Checkpoint types in h_extra for PS_CHECKPOINT; detail in h_offset.
*/
#define TERM_NORMAL	0	/* Normal termination; detail: exit status */
#define TERM_STUNNED	1	/* Process stunned; detail: pro_stun arg */
#define TERM_EXCEPTION	2	/* Promoted exception; detail: exception nr. */

/*
** Transaction buffer size used by process/segment server (in bytes)
*/
#define	PSVR_BUFSZ	/*MAX_PDSIZE*/ 8192

/*
** The following are types to distinguish between segment object numbers and
** process object numbers.  They are squeezed into the object number of
** the capability.  I hope that one day they'll go away.
*/
#define	OBJ_P	'P'
#define	OBJ_S	'S'
#define MKPRVNUM(type, num)	((objnum) (((type) << 16) | (num)))
#define PRVTYPE(obj)		((char) (((obj) >> 16) & 0xff))
#define PRVNUM(obj)		((objnum) ((obj) & 0xffff))

/* The following are used by both the kernel and the user processes so
 * they are here
 */
#define	buf_get_pd		_buf_get_pd
#define	buf_try_get_pd		_buf_try_get_pd
#define	buf_put_pd		_buf_put_pd
#define	pdmalloc		_pdmalloc
#define	pd_size			_pd_size

#ifndef ail
process_d * pdmalloc _ARGS((char *  buf, char *  bufend));
int	pd_size _ARGS((process_d *));
char *	buf_get_pd _ARGS((char *p, char *endbuf, process_d *pd));
char *	buf_put_pd _ARGS((char *p, char *endbuf, process_d *pd));
char *	buf_try_get_pd _ARGS((char *p, char *e, process_d *pd, int *nthr ));
#endif

#endif /* __PROCESS_PROC_H__ */
