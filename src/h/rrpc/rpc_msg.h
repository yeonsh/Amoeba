/*	@(#)rpc_msg.h	1.1	93/10/07 17:04:08 */
/*
 *
 * Replicated RPC Package
 *
 * Copyright 1992 Mark D. Wood
 * Vrije Universiteit, Amsterdam, Nederland
 *
 * Rights to use this source in unmodified form granted for all
 * commercial and research uses.  Rights to develop derivative
 * versions reserved by the authors.
 *
 * Message header file
 */

#include <semaphore.h>

#define MSG_MAXSIZE 3072
#define MSG_SIZE(mp) ((mp)->end_ptr - (mp)->data)
#define MSG_DATA_SIZE(mp) ((mp)->end_ptr - (mp)->start_ptr)

#define MSG_BUF_END(mp) ((mp)->data + MSG_MAXSIZE)
#define MSG_END(mp) ((mp)->end_ptr)

char *_str_malloc _ARGS((char *p));

/* home brew signals */

struct signal {
    int counter;
    semaphore block;
};

typedef struct signal SIGNAL;


/* message stuff */

struct message {
    struct message *next_msg;	/* next on freelist/queued list */
    struct message *prev_msg;	/* prev on queued list */
    int refcnt;			/* number of references to this message */
    header hdr;			/* Amoeba header */
    char *start_ptr;		/* beginning of data, past ID tag */
    char *end_ptr;		/* current end of data, plus one (place to */
				/* put new data ) */
    char *next_ptr;		/* position w/i message for retrieving */
    char data[MSG_MAXSIZE];	/* the data */
    struct contextnode *ctxtp;	/* context of sender */

				/* (following for handling RPC to group */
                                /* forwarding (only valid for original */
				/* recipient) */
    semaphore grp_sync;		/* semaphore for signaling reply */
    struct message
      *grp_forwarded_mp;	/* pointer to original message */
    struct message
      *grp_reply_mp;		/* used to pass reply sent back to thread */
				/* that originally rcvd the message */
};

typedef struct message message;

#define MAX_MSG_ENTRIES 16

struct msg_entry {
    message *(*func) _ARGS((message *)); /* function, NULL if queued */
    char *name;			/* string name of entry */
    int check_seqnum;		/* TRUE if system should check seqnum */
};

#define MSG_OFFSET_CTXTNAME 4
#define MSG_OFFSET_FORWARD  0

#define MSG_ENTRY(mp)  ((mp)->hdr.h_command)
#define MSG_SEQNUM(mp) ((mp)->hdr.h_offset)
#define MSG_OPTIONS(mp) ((mp)->hdr.h_extra)
#define MSG_SENDER_NAME(mp) ((mp)->data + MSG_OFFSET_CTXTNAME)
#define MSG_SIGNATURE(mp) (&(mp)->hdr.h_signature)

#define msg_rewind(mp) ((mp)->next_ptr = (mp)->start_ptr)
#define msg_increfcount(mp) ((mp)?(mp)->refcnt++:0)
#define msg_queued_msg_avail(msg_q) (msg_q)


#ifdef DEBUG
int msg_numalloc;		/* number of messages currently allocated */
int msg_numfree;		/* number of messages on free list */
int msg_inuse;			/* number of active allocated messages */
#endif

/* message options */

#define MSG_RPLY_FORWARD 0x100
#define MSG_REQ_FORWARD  0x200
#define MSG_ERR_FORWARD  0x400
#define MSG_NO_REPLY     0x800
#define MSG_CTXT_RMV     0x1000

