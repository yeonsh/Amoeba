/*	@(#)rpc_msgdefs.h	1.1	93/10/07 17:04:11 */
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

/* type signatures for the routines */

message * _alloc_msg();
int _check_seqnum _ARGS((message *msg));

message * msg_newmsg();
void msg_delete _ARGS((message *mp));
void msg_put _ARGS((message *mp, int type, PTR val));
void msg_put_int _ARGS((message *mp, int val));
void msg_put_real _ARGS((message *mp, float val));
void msg_put_address _ARGS((message *mp, address *val));
void msg_put_string _ARGS((message *mp, char *val));
int msg_get _ARGS((message *msg, int type, PTR val));
int msg_get_int _ARGS((message *m, int *v));
int msg_get_real _ARGS((message *m, float *v));
int msg_get_address _ARGS((message *m, address *v));
int msg_get_string _ARGS((message *m, char **v));
void msg_get_stamp _ARGS((message *mp, address **ctxt_addr_p, int *seq_num_p, char **ctxt_name));
address *msg_sender_address _ARGS((message * mp));
int msg_rpc _ARGS((address *addr, int entry, message *req_mp, message **reply));
message * msg_rpc_rcv _ARGS((address *rcv_port));
int msg_rpc_reply _ARGS((message *req_mp, message *reply_mp));
void msg_printaccess _ARGS((message *mp));
void ctx_define_entry _ARGS((int entry, message *(*func)(message *),char *entry_name));

void msg_listener _ARGS((CTXT_NODE *ctxtp));

address NULLADDRESS;

#define addr_isequal(a, b) \
		    (!memcmp((_VOIDSTAR) (a), (_VOIDSTAR) (b),sizeof(port)))
#define addr_isnull(a) (addr_isequal(a, &NULLADDRESS))

#define MSG_STACKSIZE 16384

int _t_fork _ARGS((void (*func)(void *), void *parm, char *name));

message *msg_head_of_queue _ARGS((message **head));
void msg_add_to_queue _ARGS((message **head, message *mp));

void _sig_wait _ARGS((SIGNAL *sp));
void _sig_signal _ARGS((SIGNAL *sp));
void _sig_init _ARGS((SIGNAL *sp));
void _sig_sigall _ARGS((SIGNAL *sp));

#define msg_get_port(m, v) msg_get_address(m, v)
#define msg_put_port(m, v) msg_put_address(m, v)
#define msg_sender_port(m) msg_sender_address(m)
