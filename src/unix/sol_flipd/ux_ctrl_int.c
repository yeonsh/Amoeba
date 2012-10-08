/*	@(#)ux_ctrl_int.c	1.1	96/02/27 11:49:50 */
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "assert.h"
#include "sys/debug.h"

#define ffs     string_ffs
#include "sys/flip/flip.h"
#include "sys/flip/packet.h"
#include "sys/flip/interface.h"
#undef ffs
#undef timeout

#include "ux_int.h"
#include "ux_ctrl_int.h"
#include "ux_rpc_int.h"

#define threadp	unix_threadp
#include <sys/param.h>
#include <sys/types.h>
#include <sys/user.h>
/* #include <assert.h> */
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#undef threadp

#include "fldefs.h"

INIT_ASSERT

struct ctrl_device {
    char	ctrl_buf[MAXPRINTBUF];
};

static struct ctrl_device ctrl_dev[NCTRL]; 	/* devices for ctrl */


void handle_stat(queue_t *q, mblk_t *mp)
{
    struct ctrl_args *ctrl_args;
    struct ctrl_device *ctrlfd;
    minor_t minor;
    int sum;
    unsigned char *buf;

    if ((mp->b_cont = allocb(MAXPRINTBUF, BPRI_MED)) == NULL) {
	printf("handle_ctrl: out of message blocks\n");
	qreply(q, mp);
	return;
    }

    buf = DB_BASE(mp->b_cont);
    sum = rpc_statistics(buf, buf + MAXPRINTBUF);
    sum += int_statistics(buf + sum, buf + MAXPRINTBUF);
    sum += flip_netstat(buf + sum, buf + MAXPRINTBUF);
    sum += ffstatistics(buf + sum, buf + MAXPRINTBUF);
    sum += fleth_stat(buf + sum, buf + MAXPRINTBUF);
    mp->b_cont->b_rptr = DB_BASE(mp->b_cont);
    mp->b_cont->b_wptr = mp->b_cont->b_rptr + sum;

    ctrl_args = (struct ctrl_args *) DB_BASE(mp);
    ctrl_args->ctrl_status = sum;

    mp->b_rptr = DB_BASE(mp);
    mp->b_wptr = mp->b_rptr + sizeof(struct ctrl_args);
    DB_TYPE(mp) = M_PROTO;

    qreply(q, mp);
}


void handle_dump(queue_t *q, mblk_t *mp)
{
    struct ctrl_args *ctrl_args;
    struct ctrl_device *ctrlfd;
    minor_t minor;
    int sum;
    unsigned char *buf;

    if ((mp->b_cont = allocb(MAXPRINTBUF, BPRI_MED)) == NULL) {
	printf("handle_ctrl: out of message blocks\n");
	qreply(q, mp);
	return;
    }

    buf = DB_BASE(mp->b_cont);
    sum = rpc_dump(buf, buf + MAXPRINTBUF);
    sum += port_dump(buf + sum, buf + MAXPRINTBUF);
    sum += kid_dump(buf + sum, buf + MAXPRINTBUF);
    sum += flip_netdump(buf + sum, buf + MAXPRINTBUF);
    sum += int_dump(buf + sum, buf + MAXPRINTBUF);
    sum += adr_dump(buf + sum, buf + MAXPRINTBUF);
    sum += ff_dump(buf + sum, buf + MAXPRINTBUF);
    mp->b_cont->b_rptr = DB_BASE(mp->b_cont);
    mp->b_cont->b_wptr = mp->b_cont->b_rptr + sum;

    ctrl_args = (struct ctrl_args *) DB_BASE(mp);
    ctrl_args->ctrl_status = sum;

    mp->b_rptr = DB_BASE(mp);
    mp->b_wptr = mp->b_rptr + sizeof(struct ctrl_args);
    DB_TYPE(mp) = M_PROTO;

    qreply(q, mp);
}


void handle_ctrl(queue_t *q, mblk_t *mp)
{
    printf("sorry, not implemented yet\n");
}


#if 0
void flctrl_ctrl(queue_t *q, mblk_t *mp)
{
    short nw, delay, loss, on, trusted;
    struct ctrl_device *fd;
    struct ctrl_args *fa;
    int sum;
    struct copyresp *csp;
    struct fl_q_priv *q_info;

    q_info = (struct fl_q_priv *)(q->q_ptr);
    csp = (struct copyresp *) mp->b_rptr;
    if (csp->cp_rval) {
	snd_iocnak(q, mp, EFAULT);
	return;
    }
    fa = (struct ctrl_args *) mp->b_cont->b_rptr;
    ((struct fl_q_priv *)q->q_ptr)->db = mp->b_cont;
    mp->b_cont = 0;
    fd = &ctrl_dev[((struct fl_q_priv *)q->q_ptr)->minor - FIRST_CTRL];
    
    nw= fa->ctrl_nw;
    on= fa->ctrl_on;
    loss= fa->ctrl_loss;
    delay= fa->ctrl_delay;
    trusted = fa->ctrl_trusted;
    
    if(flip_control(nw, &delay, &loss, &on, &trusted))
    {
	fa->ctrl_nw = nw;
	fa->ctrl_on = on;
	fa->ctrl_loss = loss;
	fa->ctrl_delay = delay;
	fa->ctrl_trusted = trusted;
	fa->ctrl_status = STD_OK;
    } else fa->ctrl_status = STD_SYSERR;
    copyoutreq(q, mp, (caddr_t) fa, q_info->struct_addr,
			    sizeof(struct ctrl_args), flctrl_reply);
}
#endif


/*ARGSUSED*/
int flctrl_open(
        queue_t *q,     /* pointer to the read queue */
        dev_t *devp,    /* pointer to major/minor device */
        int fflags,     /* file flags */
        int sflags,     /* stream open flags */
        cred_t *credp   /* pointer to a credentials struct */
)
{
    return(0);
}


/*ARGSUSED*/
int flctrl_close(queue_t *q, int fflags, cred_t *credp)
{
    return(0);
}
