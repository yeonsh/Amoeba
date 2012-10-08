/*	@(#)fldefs.h	1.1	96/02/27 11:49:37 */
#ifndef __FLDEFS_H__
#define __FLDEFS_H__

#include <sys/types.h>
#include <sys/cmn_err.h>
#include "thread.h"
#include "kthread.h"
#include "module/strmisc.h"
#include "module/rpc.h"

#define MYASSERT(A)	if (!(A)) {cmn_err(CE_PANIC, "file: %s, line %d\n", __FILE__, __LINE__);}

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
 
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define MAXIFS		4
#define FLIP		0x8146
#define ETH_WEIGHT	3

#define DB_BASE(mp)	((mp)->b_datap->db_base)
#define DB_TYPE(mp)	((mp)->b_datap->db_type)
#define Q_PRIV(q)	((struct fl_q_priv *)q->q_ptr)

struct cv_list_t {
	caddr_t event;
	kcondvar_t cv;
	struct cv_list_t *next;
};

struct rpc_info_t {
	msg_p msg;
	long totalvir;
	int first;
	int getdata_event;
	mblk_t *mp;
	queue_t *q;
};

struct fl_q_priv {
	/*
	caddr_t		struct_addr;
	caddr_t		struct_addr2;
	*/
	mblk_t		*db;
	/*
	mblk_t		*db2;
	*/
	mblk_t		*buf1;
	minor_t		minor;
	/*
	mblk_t		*queue_list;
	mblk_t		*last_element;
	int		queue_size;
	*/
	long		pid;
};

struct fleth {
        queue_t *q;
        int fe_ntw;
};


extern struct rpc_device rpc_dev[];
extern struct rpc_info_t rpc_info[];
extern kmutex_t flopen_mutex;
extern kmutex_t sleep_mutex;
extern kmutex_t flip_mutex;
extern struct fleth fleth[];
extern int  nifs;
extern int timeoutval;
extern int flip_unlinked;


extern int fl_wakeup(char *fe);
extern int fl_sleep(struct rpc_device *rpcfd, char *fe);
extern void rpc_initstate(thread_t *);
extern void rpc_stoprpc(thread_t *);
extern void sweeper_set(void (*)(long), long, interval, int);


#ifdef SECURITY
extern long secure_trans(header *, bufptr, f_size_t, header *, bufptr, f_size_t);
extern long secure_putrep(header *, bufptr, f_size_t);
extern long secure_getreq(header *, bufptr, f_size_t);
#endif /* SECURITY */

extern void adr_init(void);
extern void port_init(void);
extern void netswitch_init(void);
extern void rpc_init(void);
extern void kid_init(void);
extern void int_init(void);
extern void flux_failure(pkt_p);
extern void pktswitch(pkt_p, int, location *);
extern void sweeper_run(long);
extern int flopen(queue_t *, dev_t *, int, int, cred_t *);
extern int flclose(queue_t *, int, cred_t *);
extern int fluwput(queue_t *, mblk_t *);
extern int fllrput(queue_t *, mblk_t *);
extern int fluwsrv(queue_t *);
extern int fllwsrv(queue_t *);
extern int fllrsrv(queue_t *q);
extern void fl_ip_read(queue_t *, mblk_t *);
extern void fl_ip_write(queue_t *, mblk_t *);
extern void flrpc_trans(queue_t *, mblk_t *);
extern void flrpc_getreq(queue_t *, mblk_t *);
extern void flrpc_putrep(queue_t *, mblk_t *);
extern void flctrl_stat(queue_t *, mblk_t *);
extern void flctrl_dump(queue_t *, mblk_t *);
extern void flctrl_ctrl(queue_t *, mblk_t *);
extern void snd_iocnak(queue_t *, mblk_t *, int);
extern void snd_iocack(queue_t *, mblk_t *);
extern void fleth_broadcast(int, pkt_p);
extern void fleth_send(int, pkt_p, location *);
extern void fl_req_credit(int, location *, pkt_p);
extern void failure(location *, pkt_p);

#endif /* __FLDEFS_H__ */
