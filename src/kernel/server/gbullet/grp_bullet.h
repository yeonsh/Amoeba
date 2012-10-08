/*	@(#)grp_bullet.h	1.1	96/02/27 14:07:16 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define MAXGROUP	    20

/* resources message is sent at least every RES_SWEEPS sweeps */
#define RES_SWEEPS	    5

#define Seqno_t		    int16
#define buf_get_seqno	    buf_get_int16

#define hseq(seq)	    0
#define lseq(seq)	    (*(seq))
#define less_seq_no(a,b)    (*(a) < *(b))
#define eq_seq_no(a,b)      (*(a) == *(b))
#define zero_seq_no(p)	    (*(p) = 0)
#define invalid_seqno(seq)  (*(seq) < 0)

#define GRP_STACKSIZE	    16000
#define MIN_COPIES(nmem)    1

/* Number of buffers for grp_send thread */ 
#define	GRP_SEND_BUFS	    33

/* maximum size of log message buffer */
#define MAX_LOG_BUF		256

/* Returns from version compare */
#define VERS_INCOMP		-1	/* Incompatible versions */
#define VERS_EQUAL		0	/* Versions are equal */
#define VERS_ONE		1	/* Arg1 is the latest vesrion */
#define VERS_TWO		2	/* Arg2 is the latest version */

/* Global state ...*/

#define BS_OK		0		/* Global state is ok .... */
#define BS_WAITING	1		/* wait until all is ok again */
#define BS_DEAD		2		/* wait forever ... */

void message _ARGS((char *, ...));

#define scream		message
#define gpanic(list)	{ scream list; bpanic("fatal"); }

void   get_global_seqno	_ARGS((Seqno_t *seq));
char   *buf_put_seqno	_ARGS((char *p, char *e, Seqno_t *valp));

errstat grp_get_state	_ARGS((int bestid, int mode, interval maxdelay));
long	sp_mytime	_ARGS((void));

void	set_copymode	_ARGS((int mode));
int	get_copymode	_ARGS((void));
void	flush_copymode	_ARGS((void));

void	grp_got_req	_ARGS((header *hdr, char *buf, int size));
void	grp_handled_req _ARGS((void));

/* A mask indicating the members we have ever seen */
peer_bits		bs_members_seen ;

#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"

int	bs_build_group	_ARGS((void));
void	encode_superblock _ARGS((Superblk *myendian, Superblk *bigendian));
errstat gs_help_member	_ARGS((capability *cap));
errstat gs_be_welcome	_ARGS((int my_index, bufptr, bufptr));
errstat gs_kill_members _ARGS((int zombielist[]));
errstat	bs_inq_inode	_ARGS((Inodenum inode));
errstat bs_send_inodes	_ARGS((Inodenum start, Inodenum number, int rep, int locked, int mode));
void  bs_publish_inodes	_ARGS((Inodenum start, Inodenum number, int mode));
void  bs_publist	_ARGS((int)) ;
void	bs_send_resources _ARGS((int));
Inodenum bs_grp_ino_free _ARGS((int member));
disk_addr bs_grp_bl_free _ARGS((int member));
disk_addr bs_grp_blocks _ARGS((int member));
void	bs_version_incr _ARGS((Inode *));
int	bs_version_cmp	_ARGS((uint32,uint32));
