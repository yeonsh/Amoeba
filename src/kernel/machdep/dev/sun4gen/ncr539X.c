/*	@(#)ncr539X.c	1.6	96/02/27 13:54:50 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *	NCR539X SCSI Driver
 *
 *  The generic SCSI command interface (see ../generic/s[dt].c) requires
 *  that the following entry points be defined in the low level driver:
 *
 *	scsi_cmd       - execute a command on the SCSI bus and
 *                       wait until it completes.
 *	scsi_ctlr_init - set the vector for a SCSI device and
 *                       initialise the device and SCSI bus.
 *
 *  Note that the ncr539X is always programmed to be the initiator and so
 *  nothing is done to support target mode commands.  If they appear we panic.
 *
 *  Because of the nature of mutexes they can't be used for locking the
 *  controller.  When you unlock a mutex the lock goes immediately to the
 *  next in line but we need to be able to steal the lock at interrupt level.
 *  Therefore we need a lock mechanism which requires you to become runnable
 *  before you can get the lock and thus the extra locking routine.
 *
 *  Note that the SF_BUSY flag is set to indicate that DMA is in progress.
 *
 * Author:   Gregory J. Sharp, Aug 1991
 * Modified: Gregory J. Sharp, Nov 1992
 *		added openprom support.
 *	     Gregory J. Sharp, Aug 1993
 *		moved openprom support and DMA code to separate files
 *		and thus made the driver generic for sun4c and sun4m.
 *	     Gregory J. Sharp, Sep 1993
 *		added synchronous scsi support
 *	     Gregory J. Sharp, Jul 1994
 *		the one driver now handles multiple controllers of
 *		different types and is more robust on the SS1 machines.
 */

#include "amoeba.h"
#include "stdlib.h"
#include "machdep.h"
#include "cmdreg.h"
#include "stderr.h"
#include "memaccess.h"
#include "map.h"
#include "mmuproto.h"
#include "ncr539X.h"
#include "scsi_dma.h"
#include "scsi_dmaprot.h"
#include "scsi.h"
#include "scsi_sun4.h"
#include "sys/proto.h"
#include "debug.h"
#include "assert.h"
INIT_ASSERT

#ifdef DEBUG

#define	QSZ	10

static int q_head;
static struct
{
	uint8	q_is;	/* interrupt status */
	uint8	q_st;	/* status reg */
	uint8	q_ss;	/* sequence step */
	uint8	q_cs;	/* controller state */
	uint32	q_un;	/* unit that interrupted */
} q_data[QSZ];

#define	SAVE_ISTATE(is, st, ss, cs, un) \
	{					\
		q_data[q_head].q_is = is;	\
		q_data[q_head].q_st = st;	\
		q_data[q_head].q_ss = ss;	\
		q_data[q_head].q_cs = cs;	\
		q_data[q_head].q_un = un;	\
		if (++q_head >= QSZ)		\
		    q_head = 0;			\
	}

void
dump_history()
{
    int i = QSZ;
    int j = q_head;

    printf("\nhistory of interrupts\n");
    while (--i >= 0)
    {
	printf("%d: is=%02x, st=%02x, ss=%02x, cstate=%x unit = %d\n",
		QSZ - i, q_data[j].q_is, q_data[j].q_st,
		q_data[j].q_ss, q_data[j].q_cs, q_data[j].q_un);
	if (++j == QSZ)
	    j = 0;
    }
}

uint8 Last_mesgout;
#define	SAVE_LAST_MESGO(a)	do { Last_mesgout = a; } while(0)

#else /* DEBUG */

#define	SAVE_ISTATE(is, st, ss, cs, un)
#define	dump_history()
#define	SAVE_LAST_MESGO(a)

#endif /* DEBUG */


#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#define	MAX(a, b)	((a) > (b) ? (a) : (b))

/* We use the can disconnect flag to mean that target accepts messages */
#define	SF_CAN_MESGS	SF_CAN_DISCON
#define	SF_FASTSCSI	SF_DMA_WANTED

/* The following are declared in scsi_dev.c, where they are initialised! */
extern scsi_ctlr	Scsi_ctlr_tab[];
extern scsi_sun4info	Scsi_info[];
extern unsigned int	Num_scsi_ctlrs;

/* From the generic SCSI code */
extern unsigned int	Sd_Lim_blk_cnt;

#ifdef NO_SYNCHRONOUS_SCSI
#undef NCR_SO_MAX_OFFSET
#define	NCR_SO_MAX_OFFSET	0
#endif /* NO_SYNCHRONOUS_SCSI */

/*
 * The Scsi_info struct is not filled in statically as with other systems
 * since it must be read from the PROM at initialization of the driver.
 *  In the presence of multiple SCSI devices, the numbering of devices
 *  is an issue that must be sorted out later.
 */

#define ADR539X(c)		((ncr_539X *) Scsi_info[c].s_scsi_ctlr)
#define ADR_DMACTLR(c)		((dmactlr *) Scsi_info[c].s_dma_ctlr)
#define INITIATOR_ID(c)		Scsi_info[c].s_initiator_id
#define CLOCK_CONV_FACTOR(c)	Scsi_info[c].s_clk_cnv
#define	CLOCK_FREQ(c)		Scsi_info[c].s_clockfreq
#define NCR_SEL_TIMEOUT(c)	Scsi_info[c].s_sel_timeout

/*
 * The reset delay is the time in msecs to wait after a reset before allowing
 * the command to start.  This is to give the SCSI devices a chance to reinit
 */
#define	RESET_TIMEOUT		4000

/*
 * The following are valid states for the c_state field.
 */
#define	STATE_DISCONNECTED	0x0
#define	STATE_SELECT		0x1
#define	STATE_CMD_CMPLT_SEQ	0x2
#define	STATE_MESG_CMD_CMPLT	0x3
#define	STATE_XFERING_INFO	0x4
#define	STATE_PHASE_CTL		0x5
#define STATE_GETMESG		0x6
#define	STATE_RESELECTED	0x7


/****************************************/
/*	Locking Code			*/
/****************************************/

static void
scsi_initlock(c)
scsi_ctlr *	c;
{
    c->c_mtx = 0;
}


/*
 * The unlock routine can't be used at interrupt level.  The lock routine can
 * only be used at interrupt level if noretry is set.  Since we must twiddle
 * locks at interrupt level the interrupt handlers also have code that fiddles
 * the locks.
 */
static int
scsi_lock(c, noretry)
scsi_ctlr *	c;
int		noretry;	/* If set then the lock must be free */
{
    for (;;)
    {
	disable();
	if (c->c_mtx == 0)
	{
	    /* We got the lock */
	    c->c_mtx = 1;
	    enable();
	    return 1;
	}
	enable();
	if (noretry)
	    return 0;
	(void) await((event) c, (interval) 0);
    }
}


static void
scsi_unlock(c)
scsi_ctlr *	c;
{
    c->c_mtx = 0;
    wakeup((event) c);
}


/****************************************/
/*	NCR53C9X SCSI Chip Routines	*/
/****************************************/

/*
 * A little routine to wakeup the top level when a command completes
 * either successfully or otherwise
 */

static void
command_done(p)
long	p;
{
    put_OR_l(&((scsi_unit *) p)->u_flags, SF_DONE);
    wakeup((event) p);
}


/*
 * clean_disconnects
 *	Wakeup the disconnected commands after a bus reset and get
 *	them to try again.  We also set a time delay of 4 seconds before
 *	the next command can run to give everyone time to recover.
 */

static void
clean_disconnects(ctlr)
int32	ctlr;
{
    volatile scsi_unit * p;
    scsi_ctlr *		 c;
    int			 unit;

    c = &Scsi_ctlr_tab[ctlr];

    /* Let the poor devices have a few seconds to recover - especially tapes */
    c->c_lbolt = getmilli() + RESET_TIMEOUT;

    /* Do the cleanup */
    for (p = c->c_discon; p != 0; p = p->u_nxtdcon)
    {
	if (p->u_devtype == INQ_SEQ_ACCESS)
	    p->u_errstat = STD_SYSERR;
	else
	    p->u_errstat = SERR_AGAIN;
	command_done((long) p);
    }

    /* For all known units, mark them in need of synchronous negotiation */
    for (unit = 0; unit < MAX_UNITS_PER_CTLR; unit++)
    {
	if ((p = c->c_unit[unit]) != 0)
	{
	    if (getl(&p->u_flags) & SF_CAN_MESGS)
		put_OR_l(&p->u_flags, SF_NEGSYNC);
	    p->u_syn_off = 0;
	}
    }
    c->c_discon = 0;
    c->c_lmioff = 0;
    c->c_state = STATE_DISCONNECTED;
}


/*
 * Can we work out what sort of chip it is?
 */
#define	TESTPAT2	(NCR_CONF2_REG_PARITY_ENABLE | NCR_CONF2_SCSI_2)
#define	TESTPAT3	(NCR_CONF3_CDB10 | NCR_CONF3_FAST_CLK)

static void
check_type(ctlr)
int32	ctlr;
{
    ncr_539X *	sbc;
    scsi_ctlr *	c;

    DPRINTF(0, ("Check chip type ctlr %d\n", ctlr));

    sbc = ADR539X(ctlr);
    c = &Scsi_ctlr_tab[ctlr];

    putb(&sbc->w.sc_config2, TESTPAT2);
    if ((getb(&sbc->r.sc_config2) & 0xF) != TESTPAT2)
    {
	DPRINTF(-1, ("scsi %d is an NCR53C90\n", ctlr));
	c->c_type = CHIP_NCR5390;
    }
    else
    {
	/* It has at least 2 config registers! */
	putb(&sbc->w.sc_config3, TESTPAT3);
	if ((getb(&sbc->r.sc_config3) & 0xF) != TESTPAT3)
	{
	    DPRINTF(-1, ("scsi %d is an NCR53C90A/B\n", ctlr));
	    c->c_type = CHIP_NCR5390A;
	}
	else
	{
	    char conf;

	    /* It has 3 config registers */
	    DPRINTF(-1, ("scsi %d is an NCR53C9X\n", ctlr));
	    c->c_type = CHIP_NCR539X;
	    /* Read out the chip revision info. */
	    putb(&sbc->w.sc_config2, NCR_CONF2_FEATURES_ENABLE);
	    putb(&sbc->w.sc_cmd, NCR_CMD_NOP | NCR_CMD_ENABLE_DMA);
	    conf = getb(&sbc->w.sc_xfr_cnt_hi);
	    DPRINTF(-1, ("Chip type = %x, revision %x\n",
					conf >> 3, conf & NCR_CHIP_REV_LEVEL));
	}
    }
}


/*
 * Reset the ncr539X chip
 */

static void
ncr539X_reset(ctlr)
int32	ctlr;
{
    ncr_539X *	sbc;

    DPRINTF(0, ("Resetting SCSI ctlr %d\n", ctlr));

    sbc = ADR539X(ctlr);

    /* According to the manual you should throw a NOP in after a chip reset */
    putb(&sbc->w.sc_cmd, NCR_CMD_RESET_CHIP);
    putb(&sbc->w.sc_cmd, NCR_CMD_NOP);

    /* Do something about disconnected targets ... */
    clean_disconnects(ctlr);

    /* Reinit the bits of the chip that (might) need it */
    putb(&sbc->w.sc_clkcnv, (char) CLOCK_CONV_FACTOR(ctlr));
    putb(&sbc->w.sc_seltimo, NCR_SEL_TIMEOUT(ctlr));
    putb(&sbc->w.sc_config1,
		(char) (INITIATOR_ID(ctlr) /*| NCR_CONF1_PARITY_ENABLE */));
    NCR539X_CHIP_INIT(sbc, CLOCK_FREQ(ctlr), Scsi_ctlr_tab[ctlr].c_type);
    putb(&sbc->w.sc_cmd, NCR_CMD_SEL_ENABLE);
}


/*
 * Reset the scsi bus but not the ncr539X chip.
 */

static void
scsi_bus_reset(ctlr)
int32	ctlr;
{
    ncr_539X *	sbc;

    sbc = ADR539X(ctlr);
    DPRINTF(0, ("Resetting the SCSI bus of ctlr %d\n", ctlr));
    putb(&sbc->w.sc_cmd, NCR_CMD_RESET_BUS);
    clean_disconnects(ctlr);
    putb(&sbc->w.sc_cmd, NCR_CMD_SEL_ENABLE);
}


/*
 * Flush the fifo of the SCSI chip.  Alas, on older chips we need a small
 * delay after the flush before stuffing things into the fifo.
 */

#define	FLUSH_FIFO(sbc) \
	do {						\
	    volatile int _vol;				\
	    putb(&sbc->w.sc_cmd, NCR_CMD_FLUSH_FIFO);	\
	    _vol = 1;					\
	} while (0)


/*
 * Put the SCSI command into the fifo
 */
#define	PUT_CMD_IN_FIFO(sbc, p)	\
	do {						\
	    char *	_cp;				\
	    int		_i;				\
	    DPRINTF(0, ("putcmd\n"));			\
	    switch ((p)->u_cmdsz)			\
	    {						\
	    case 6:					\
		_cp = (char *) &(p)->u_cmd.c_6;		\
		break;					\
	    case 10:					\
		_cp = (char *) &(p)->u_cmd.c_10;	\
		break;					\
	    case 12:					\
		_cp = (char *) &(p)->u_cmd.c_12;	\
		break;					\
	    }						\
	    for (_i = 0; _i < (p)->u_cmdsz; _i++)	\
		putb(&sbc->w.sc_fifo, *(_cp + _i));	\
	} while (0)


/****************************************/
/*	Finite state machine routines	*/
/****************************************/

/*
 *  start_xfer_data
 *	Set up the DMA and start it running.  This should not be called
 *	until the SCSI device really wants to do I/O (ie. DATA IN/OUT phase).
 */

static void
start_xfer_data(c)
scsi_ctlr *	c;
{
    int32	ctlr;
    ncr_539X *	sbc;
    scsi_unit *	p;
    dmactlr *	dc;

    p = (scsi_unit *) c->c_current;
    ctlr = p->u_ctlr;
    sbc = ADR539X(ctlr);
    dc = ADR_DMACTLR(ctlr);

    DPRINTF(1, ("xfer data: dp= %lx, sz = %d\n", p->u_data, p->u_datasz));
    if (p->u_datasz == 0)
    {
	printf("scsi(%d %d): start_xfer_data: attempt to do zero length xfer\n",
			ctlr, p->u_unit);
	p->u_errstat = STD_SYSERR;
	scsi_bus_reset(ctlr);
	enqueue(command_done, (long) p);
	return;
    }

    /* Set up the DMA for unit 'p'. */
    scsi_dma_setup(dc, (int) (getl(&p->u_flags) & SF_WRITE),
				    (uint32) p->u_data, (uint32) p->u_datasz);

    /*
     * This seems to be a requirement for making older sun4c machines work
     * properly - probably a bug in the esp100.  It means no synchronous
     * SCSI for those machines with the early chips.
     */
    if (c->c_type == CHIP_NCR5390)
    {
	assert(c->c_max_so == 0);
	FLUSH_FIFO(sbc);
    }

    put_OR_l(&p->u_flags, SF_BUSY);

    c->c_state = STATE_XFERING_INFO;

    SET_SCSI_BYTE_CNT(sbc, p->u_datasz, c->c_type);

    /* Start the command running. We always use DMA */
    putb(&sbc->w.sc_cmd, NCR_CMD_XFER_INFO | NCR_CMD_ENABLE_DMA);

    /* Enable interrupts & DMA controller */
    scsi_dma_go(dc);
}


/*
 * Handle the reselect interrupt.
 * Note, this is called from the low-level interrupt routine so enqueue
 * dangerous actions!
 */

static void
scsi_reselect(ctlr)
int32	ctlr;
{
    uint8	sel_id;
    uint8	mesg_in;
    scsi_ctlr *	c;
    scsi_unit *	p;
    volatile scsi_unit *	q;
    int		unit;
    int		target;
    ncr_539X *	sbc;
#ifdef DEBUG
    int		sel_id_bak;
#endif /* DEBUG */

    /*
     * We have been reselected.  There should be two bytes in the
     * FIFO for us since we enabled re/selection without DMA.
     * Now we need to resetup the disconnected DMA and then ACK the
     * message byte for the reselect.  When the interrupt for the ACK
     * arrives we restart the DMA.
     *
     * Note that it is not necessary to disable further reselects since
     * while we have the SCSI bus they can't happen.
     */
    DPRINTF(2, ("scsi_reselect: %d\n", ctlr));
    c = &Scsi_ctlr_tab[ctlr];
    assert(c != 0);

    /* Ensure we have control of the SCSI chip */
    if (c->c_mtx != 1)
    {
	panic("scsi_reselect: reselected without mutex\n");
	/*NOTREACHED*/
    }

    sbc = ADR539X(ctlr);
    sel_id = getb(&sbc->r.sc_fifo);
#ifdef DEBUG
    sel_id_bak = sel_id;
#endif /* DEBUG */
    mesg_in = getb(&sbc->r.sc_fifo);
    sel_id &= ~(1 << INITIATOR_ID(ctlr));
    /*
     * Make sure that exactly one target id was set and that the message
     * received was an identify message.
     * The following may well be unnecessary, but the doco doesn't say that
     * the chip checks for us.  We don't yet know who to wakeup in the event
     * of a failure so their timeout will have to get them through.
     */
    if (sel_id == 0 || (sel_id & (sel_id - 1)) || !(mesg_in & SC_MESS_IDEN))
    {
	printf("scsi_reselect: invalid reselect ctlr %d (selid=0x%x, mesg=0x%x\n", ctlr, sel_id, mesg_in);
	printf("scsi_reselect: initiator id = %d\n", INITIATOR_ID(ctlr));
	c->c_state = STATE_DISCONNECTED;
	putb(&sbc->w.sc_cmd, NCR_CMD_SEL_ENABLE);
	return;
    }

    for (target = 0; target < 8 && sel_id != 1; target++)
	sel_id >>= 1;
    assert(target != 8);
    unit = (target << 3) | (mesg_in & 07);
    p = c->c_unit[unit];
#ifdef DEBUG
    if (p == 0)
    {
	printf("scsi_resel: ctlr %d got reselect from unit %d\n", ctlr, unit);
	printf("scsi_resel: sel_id was 0x%x, id_byte = 0x%x\n",
							sel_id_bak, mesg_in);
	assert(p);
    }
#endif /* DEBUG */
    if (!(getl(&p->u_flags) & SF_DISCON))
    {
	printf("scsi_reselect: ctlr %d got reselect from target %d, unit %d\nbut it was not disconnected\n", ctlr, target, mesg_in & 07);
	c->c_state = STATE_DISCONNECTED;
	p->u_errstat = STD_SYSERR;
	enqueue(command_done, (long) p);
	return;
    }

    /* Take the unit off the disconnect list and reattach it */
    q = c->c_discon;
    assert(q);
    if (q == p)
	c->c_discon = p->u_nxtdcon;
    else
    {
	while (q != 0 && q->u_nxtdcon != p)
	    q = q->u_nxtdcon;
	assert(q != 0);
	q->u_nxtdcon = p->u_nxtdcon;
    }
    put_AND_l(&p->u_flags, ~SF_DISCON);

    /* Restore the current pointer to the reselected logical unit */
    c->c_current = p;

    putb(&sbc->w.sc_sync_offset, p->u_syn_off);
    putb(&sbc->w.sc_sync_period, p->u_syn_tp & NCR_STP_SYNC_TP);

    DPRINTF(2, ("scsi_reselect: done\n"));
    /* ACK the identify message byte */
    c->c_state = STATE_PHASE_CTL;
    putb(&sbc->w.sc_cmd, NCR_CMD_MSG_ACCEPTED);
    /* We restart the DMA after the message accepted interrupt. */
}


/*
 * Functions to convert NCR's transfer period register value to what the
 * SCSI SDTR message requires and back again.
 *  Tp = (regval * clock_period_in_nanosecs) / 4.
 *     = (regval * 1000)/(4 * clock_requency_in_MHz)
 */

uint8
conv_tpreg_to_scsi(val, clk)
uint8	val;
uint32	clk;
{
    return (val * 250) / clk;
}


uint8
conv_scsi_to_tpreg(val, clk, minperiod)
uint8	val;
uint32	clk;
int	minperiod;
{
    uint32 ret;

    ret = (val * clk) / 250;
    /* Never return less than the minimum tp the chip can handle */
    if (ret < minperiod)
	return minperiod;
    /* Catch any round-off errors */
    if (conv_tpreg_to_scsi((uint8) ret, clk) < val)
	ret++;
    return ret;
}


static void
load_sync_data(sbc, p)
ncr_539X *	sbc;
scsi_unit *	p;
{
    uint8	tp;

    tp = conv_tpreg_to_scsi(p->u_syn_tp, CLOCK_FREQ(p->u_ctlr));

    DPRINTF(0, ("Loading sync data: period = %d, offset = %d\n",
							tp, p->u_syn_off));
    /* Put the message into the fifo */
    FLUSH_FIFO(sbc);
    put_AND_l(&p->u_flags, ~SF_NEGSYNC);
    putb(&sbc->w.sc_fifo, SC_MESS_EXT);
    putb(&sbc->w.sc_fifo, 3);
    putb(&sbc->w.sc_fifo, SC_EXM_SDTR);
    putb(&sbc->w.sc_fifo, tp);
    putb(&sbc->w.sc_fifo, p->u_syn_off);
    SAVE_LAST_MESGO(SC_MESS_EXT);
}


static void
analyze_mesg_in(sbc, c, p)
ncr_539X *	sbc;
scsi_ctlr *	c;
scsi_unit *	p;
{
    c->c_state = STATE_PHASE_CTL;
    c->c_lmibuf[c->c_lmioff] = getb(&sbc->r.sc_fifo) & 0xFF;
    switch (c->c_lmibuf[0])
    {
    case SC_MESS_EXT:
	DPRINTF(0, ("scsi_intr: extended msg byte %d = %d from unit %d\n",
			    c->c_lmioff, c->c_lmibuf[c->c_lmioff], p->u_unit));
	c->c_lmioff++;
	if (c->c_lmioff > 1 && c->c_lmioff == c->c_lmibuf[1] + 2)
	{
	    /* We now have the whole message */
	    c->c_lmioff = 0;
	    if (p->u_syn_tp > NCR_STP_MAX_PERIOD)
	    {
		/*
		 * If we've already seen that the target unit's transfer
		 * period is too long for us we skip looking again.  We've
		 * already told him no synchronous scsi
		 */
		DPRINTF(0, ("scsi_intr: skipping resent message\n"));
		break;
	    }
	    if (c->c_lmibuf[2] == SC_EXM_SDTR && c->c_lmibuf[1] == 3)
	    {
		uint32 clk = CLOCK_FREQ(p->u_ctlr);

		p->u_syn_tp = conv_scsi_to_tpreg(c->c_lmibuf[3], clk,
								c->c_minperiod);
		p->u_syn_off = MIN((int) c->c_lmibuf[4], c->c_max_so);
		if (p->u_syn_tp > NCR_STP_MAX_PERIOD)
		{
		    /*
		     * Even if we have already sent our synchronous params
		     * we have to send them again because the target's period
		     * is too high for us to handle.  We mustn't try to
		     * negotiate synchronous again either!
		     */
		    DPRINTF(0, ("resending sync params\n"));
		    putb(&sbc->w.sc_cmd, NCR_CMD_SET_ATN);
		    p->u_syn_off = 0;
		    load_sync_data(sbc, p);
		}
		else
		{
		    if (getl(&p->u_flags) & SF_NEGSYNC)  /* send our params */
		    {
			DPRINTF(0, ("send sync params 1st time\n"));
			putb(&sbc->w.sc_cmd, NCR_CMD_SET_ATN);
			load_sync_data(sbc, p);
		    }
		}
		if (p->u_syn_off != 0)
		{
		    if (clk >= 40 && p->u_syn_tp < 8)
			put_OR_l(&p->u_flags, SF_FASTSCSI);
		    else
			put_AND_l(&p->u_flags, ~SF_FASTSCSI);
		    printf("scsi %2ld, unit %2d: synchronous\n",
							p->u_ctlr, p->u_unit);
		}
	    }
	    else
	    {
		DPRINTF(2, ("scsi_intr: rejecting extended message\n"));
		/* Reject the message - we can't handle it */
		putb(&sbc->w.sc_cmd, NCR_CMD_SET_ATN);
		FLUSH_FIFO(sbc);
		putb(&sbc->w.sc_fifo, (char) SC_MESS_REJ);
		SAVE_LAST_MESGO(SC_MESS_REJ);
	    }
	}
	break;

    case SC_MESS_SDP:
	/*
	 * We already read out the contents of the remaining byte count
	 * when we stopped the DMA, so there is no work to do.
	 */
	DPRINTF(2, ("scsi_intr: got SDP message from unit %d\n", p->u_unit));
	break;

    case SC_MESS_RDP:
	DPRINTF(2, ("scsi_intr: got RDP message from unit %d\n", p->u_unit));
	break;

    case SC_MESS_DIS:
	DPRINTF(2, ("scsi_intr: got DISCON message from unit %d\n", p->u_unit));
	put_OR_l(&p->u_flags, SF_DISCONMESG);
	break;

    case SC_MESS_REJ:
	/* The only message we send likely to be rejected is SC_EXM_SDTR. */
#ifdef DEBUG
	DPRINTF(2, ("Last mesg out was 0x%x,  ", Last_mesgout));
#endif /* DEBUG */
	DPRINTF(2, ("scsi_intr: got REJECT message from unit %d\n", p->u_unit));
	p->u_syn_off = 0;
	if (c->c_type != CHIP_NCR539X)
	{
	    /* If SDTR is rejected then we will find ourselves back in mesg_out
	     * phase so we send an identify message to clear the phase.  If it
	     * then rejects the identify message our goose is cooked.
	     */
	    FLUSH_FIFO(sbc);
	    putb(&sbc->w.sc_fifo,
		    SC_MESS_IDEN | SCM_DISCON | (p->u_unit & SCM_LUNIT_MASK));
	    SAVE_LAST_MESGO(SC_MESS_IDEN);
	}
	else
	{
	    /* a 539X has a reset ATN command */
	    putb(&sbc->w.sc_cmd, NCR_CMD_RESET_ATN);
	}
	break;

    default:
	/* Other messages are beyond our grasp.  We reject the message. */
	printf("scsi_intr: ctlr %d unit %d: Got unexpected message %x\n",
			p->u_ctlr, p->u_unit, c->c_lmibuf[0]);
	putb(&sbc->w.sc_cmd, NCR_CMD_SET_ATN);
	FLUSH_FIFO(sbc);
	putb(&sbc->w.sc_fifo, (char) SC_MESS_REJ);
	SAVE_LAST_MESGO(SC_MESS_REJ);
	break;
    }
    /* Acknowledge the message */
    DPRINTF(0, ("mesg ack\n"));
    putb(&sbc->w.sc_cmd, NCR_CMD_MSG_ACCEPTED);
}


static void
handle_intr(c, ctlr, sbc, seq_step, status, intr_stat)
scsi_ctlr *	c;
int32		ctlr;
ncr_539X *	sbc;
int		intr_stat;
int		status;
int		seq_step;
{
    scsi_unit * p;
    int32	xfer_cnt;

    if (intr_stat & NCR_ISTAT_SCSI_RESET_DETECTED)
    {
	DPRINTF(0, ("scsi_intr: got a reset interrupt on ctlr %d\n", ctlr));
	clean_disconnects(ctlr);
	putb(&sbc->w.sc_cmd, NCR_CMD_SEL_ENABLE);
	return;
    }

    p = (scsi_unit *) c->c_current;
    assert(p != 0);
    SAVE_ISTATE(intr_stat, status, seq_step, c->c_state, p->u_unit);
    DPRINTF(1, ("c=%d u=%02d: s=%02x, ss=%x, is=%02x, st=%x\n",
		    ctlr, p->u_unit, status, seq_step, intr_stat, c->c_state));

    if (getl(&p->u_flags) & SF_BUSY)
    {
	/*
	 * Something interrupted the DMA.  Stop it! We do this very early in
	 * case a device allows bogus REQ/ACKs to occur and distort the DMA
	 * bytecount.  We pick it up later if we were Xferring data.
	 */
	scsi_dma_stop(ADR_DMACTLR(ctlr));
	xfer_cnt = GET_SCSI_BYTE_CNT(sbc, c->c_type);
	DPRINTF(0, ("scsi_intr: unit %d: DMA stopped, 0x%d bytes to go.\n",
							p->u_unit, xfer_cnt));
	put_AND_l(&p->u_flags, ~SF_BUSY);
    }

    if (status & (NCR_STAT_PARITY_ERR | NCR_STAT_GROSS_ERR))
    {
	printf("scsi_intr: ctlr %d got gross or parity error (st=%x), state = %d\n",
					ctlr, status, c->c_state);
	c->c_state = STATE_DISCONNECTED;
	p->u_errstat = STD_SYSERR;
	enqueue(scsi_bus_reset, ctlr);
	enqueue(command_done, (long) p);
	putb(&sbc->w.sc_cmd, NCR_CMD_SEL_ENABLE);
	return;
    }

    /* Look for reselect very early to speed service time */
    if (intr_stat & NCR_ISTAT_RESELECTED)
    {
	DPRINTF(2, ("scsi_intr: ctlr %d was reselected\n", ctlr));

	/* Sanity checks */
	if (!(intr_stat & NCR_ISTAT_FUNC_CMPLT))
	{
	    DPRINTF(0,
	    ("scsi_intr: ctlr %d got reselect without func cplt, state = %d\n",
							    ctlr, c->c_state));
	    /* We've probably already handled this one */
	    return;
	}

	if (c->c_state == STATE_SELECT)
	{
	    /*
	     * Whoever tried to select lost control to the reselecting target.
	     * We take the controller so the reselect can use it.
	     * Then we wakeup the failed select so it can retry.
	     */
	    assert(c->c_mtx == 1);
	    p->u_errstat = SERR_RESELECT_PENDING;
	    put_OR_l(&p->u_flags, SF_DONE);
	    enqueue((void (*)()) wakeup, (long) p);
	}
	else
	{
	    c->c_mtx = 1;
	    /* We've already handled this interrupt? */
	    if (c->c_state != STATE_DISCONNECTED)
	    {
		dump_history();
		DPRINTF(0, ("scsi_intr: ctlr = %d spurious reconnect intr (state = %d)\n", ctlr, c->c_state));
		DPRINTF(0, ("scsi_intr: s=%02x ss=%x is=%02x, st=%x\n",
				    status, seq_step, intr_stat, c->c_state));
		return;
	    }
	}
	c->c_state = STATE_RESELECTED;
	scsi_reselect(ctlr);
	return;
    }

    if (intr_stat & (NCR_ISTAT_SELECTED | NCR_ISTAT_SELECTED_ATN))
    {
	/* We will ignore the select and hope it goes away */
	printf("scsi_intr: ctlr %d got a select while initiator!\n", ctlr);
	return;
    }

    if (intr_stat & NCR_ISTAT_ILLEGAL_CMD)
    {
	DPRINTF(-1, ("scsi_intr: ctlr %d got illegal SCSI command\n", ctlr));
	DPRINTF(-1, ("state = %x, s =%x, ss =%x, is =%x\n",
				    c->c_state, status, seq_step, intr_stat));
	if (c->c_state == STATE_RESELECTED || c->c_state == STATE_SELECT)
	{
	    p->u_errstat = SERR_RESELECT_PENDING;
	    c->c_mtx = 0;
	    enqueue((void (*)()) wakeup, (long) c);
	    put_OR_l(&p->u_flags, SF_DONE);
	    enqueue((void (*)()) wakeup, (long) p);
	}
	else
	{
	    p->u_errstat = SERR_AGAIN;
	    enqueue(command_done, (long) p);
	}
	c->c_state = STATE_DISCONNECTED;
	return;
    }

    /* If we are disconnected only (re)selects are "legal" interrupts */
    if (c->c_state == STATE_DISCONNECTED)
    {
	printf("scsi_intr: spurious interrupt on ctlr %d\n", ctlr);
	printf("scsi_intr: is=%x, ss=%x, st=%x\n",
					    intr_stat, seq_step, status);
	return;
    }

    /* Look for disconnect */
    if (intr_stat & NCR_ISTAT_DISCONNECT)
    {
	switch (c->c_state)
	{
	case STATE_SELECT:
	    DPRINTF(-1, ("scsi_intr: selection failed for ctlr %d unit %d\n",
							    ctlr, p->u_unit));
	    DPRINTF(-1, ("s=%x, ss=%x, is=%x\n", status, seq_step, intr_stat));
	    if (getl(&p->u_flags) & SF_NEGSYNC)
	    {
		/* Hack for devices that disconnect if we try synch neg */
		put_AND_l(&p->u_flags, ~SF_NEGSYNC);
		p->u_errstat = SERR_AGAIN;
	    }
	    else
		p->u_errstat = SERR_SELFAIL;
	    /* Fall through */

	case STATE_MESG_CMD_CMPLT:
	    enqueue(command_done, (long) p);
	    break;

	default:
	    if (!(getl(&p->u_flags) & SF_DISCONMESG))
	    {
		/* Customer disconnected without a disconnect message! */
		printf("scsi_intr: ctlr %d unit %d: target", ctlr, p->u_unit);
		printf(" disconnected without warning, State=%d\n", c->c_state);
		dump_history();
		p->u_errstat = STD_SYSERR;
#if 1
		enqueue(scsi_bus_reset, ctlr);
#endif
		enqueue(command_done, (long) p);
	    }
	    else
	    {
		/* Put it on the list of disconnected units */
		p->u_nxtdcon = c->c_discon;
		c->c_discon = p;
		put_AND_l(&p->u_flags, ~SF_DISCONMESG);
	    }
	}

	/* Unlock the SCSI chip - we can't call scsi_unlock here */
	c->c_mtx = 0;
	enqueue((void (*)()) wakeup, (long) c);
	/* After a discon we enable reselects */
	putb(&sbc->w.sc_cmd, NCR_CMD_SEL_ENABLE);
	put_OR_l(&p->u_flags, SF_DISCON);

	c->c_state = STATE_DISCONNECTED;
	return;
    }

    /*
     * The only other bits that can be on in the interrupt status register are
     * bus service and function complete.  Check the current state.
     */

    switch (c->c_state)
    {
    case STATE_SELECT:
	DPRINTF(2, ("scsi_intr: in SELECT state\n"));
	/*
	 * The disconnect is dealt with so the following must be true or
	 * the hardware is broken.
	 */
	assert(intr_stat == (NCR_ISTAT_BUS_SERVICE | NCR_ISTAT_FUNC_CMPLT));
	put_AND_l(&p->u_flags, ~SF_SELECTING);
	switch (seq_step)
	{
	case 0:
	    /*
	     * Target doesn't deal with messages so skip trying to allow
	     * disconnect and retry the whole command.
	     */
	    DPRINTF(0, ("scsi_intr: ctlr %d: unit %d won't listen to ATN\n",
							    ctlr, p->u_unit));
	    put_AND_l(&p->u_flags, ~(SF_CAN_MESGS | SF_NEGSYNC));
	    p->u_errstat = SERR_AGAIN;
	    enqueue(scsi_bus_reset, ctlr);
	    enqueue(command_done, (long) p);
	    c->c_state = STATE_DISCONNECTED;
	    return;

	case 1:
	    assert(getl(&p->u_flags) & SF_NEGSYNC);
	    if ((status & NCR_BP_MASK) == NCR_BP_MESGO)
	    {
		DPRINTF(0, ("scsi_intr: ctlr %d: neg sync\n", ctlr));
		p->u_syn_tp = c->c_minperiod;
		p->u_syn_off = c->c_max_so;
		load_sync_data(sbc, p);
	    }
	    else
		put_AND_l(&p->u_flags, ~SF_NEGSYNC); /* It doesn't want to talk!? */
	    /* Go on to look at bus phase */
	    break;

	case 3:
	    /* Else nasty error of some sort.  Find out what target wants. */
	    {
		uint8 fflgs;
		int i;
		uint8 * cp;
		
		fflgs = getb(&sbc->r.sc_fifoflags) & NCR_FIFO_FLAGS;
		if (fflgs != 0)
		{
		    /* Not all the command bytes were sent - clean up */
		    printf("scsi_intr: ctlr %d unit %d cmd phase interrupted\n",
						ctlr, p->u_unit);
		    printf("st=%x, is=%x, ss=%x, fflags = %x\n",
					status, intr_stat, seq_step, fflgs);
		    printf("current status = %x\n", getb(&sbc->r.sc_stat));
		    printf("cmdsz = %d, cmd =", p->u_cmdsz);
		    cp = (uint8 *) &p->u_cmd.c_6;
		    for (i = 0; i < p->u_cmdsz; i++)
			printf(" 0x%x", cp[i]);
		    printf("\n");
		    p->u_errstat = SERR_AGAIN;
		    /* If it is in status phase then check it out */
		    if ((status & NCR_BP_MASK) != NCR_BP_STATUS)
		    {
			/* Otherwise abort it */
			enqueue(scsi_bus_reset, ctlr);
			enqueue(command_done, (long) p);
			c->c_state = STATE_DISCONNECTED;
			return;
		    }
		}
	    }
	    break;

#if 0 /* Optimizer doesn't optimise out these do-nothings */
	case 2:
	    /* We didn't get cmd phase, so see what the target does want */
	case 4:
	    /* Select is OK - go on to look at bus phase */
	    break;
#endif
	}
	break;

    case STATE_CMD_CMPLT_SEQ:
	DPRINTF(2, ("scsi_intr: in COMMAND_COMPLETE state\n"));
	/* Fetch the status and the message from the FIFO */
	if (intr_stat == NCR_ISTAT_FUNC_CMPLT ||
					    intr_stat == NCR_ISTAT_BUS_SERVICE)
	{
	    uint8 stat_byte;
	    uint8 mesg_byte;

	    assert(!(getl(&p->u_flags) & SF_BUSY));
	    stat_byte = getb(&sbc->r.sc_fifo);
	    mesg_byte = getb(&sbc->r.sc_fifo);
	    /* Analyze result and set error states */
	    if (stat_byte != STAT_GOOD || mesg_byte != SC_MESS_CC)
	    {
		if (stat_byte == STAT_CHECK_CON)
		     p->u_errstat = SERR_CHECKCON;
		else
		{
		    if (stat_byte == STAT_BUSY)
		    {
			DPRINTF(-1, ("scsi_intr: ctlr %d, unit %d device busy\n",
				ctlr, p->u_unit));
			p->u_errstat = SERR_AGAIN;
		    }
		    else
		    {
			p->u_errstat = SERR_CMDFAIL;
			printf("scsi_intr: cmd status = %x, mesg = %x\n",
							stat_byte, mesg_byte);
		    }
		}
	    }
	    c->c_state = STATE_MESG_CMD_CMPLT;
	    DPRINTF(0, ("cmdcmplt ack\n"));
	    putb(&sbc->w.sc_cmd, NCR_CMD_MSG_ACCEPTED);
	}
	else
	{
	    printf("scsi_intr: command complete sequence failed for ctlr %d\n",
									ctlr);
	    p->u_errstat = STD_SYSERR;
	    enqueue(command_done, (long) p);
	}
	return;

    case STATE_MESG_CMD_CMPLT:
	/* If we get here something is wrong.  It should have disconnected */
	printf("scsi_intr: ctlr %d unit %d: no disconnect after cmd cmplt\n",
							    ctlr, p->u_unit);
	printf("scsi_intr: is=%x, sr=%x ss=%x\n", intr_stat, status, seq_step);
	p->u_errstat = STD_SYSERR;
	enqueue(scsi_bus_reset, ctlr);
	enqueue(command_done, (long) p);
	return;

    case STATE_XFERING_INFO:
	/* We've already stopped the DMA above - let's look at the result */
	if (xfer_cnt & 0x1FF)
	{
	    /*
	     * HACK ALERT:  Some devices have let the DMA run on for up to the
	     * length of the FIFO.  Therefore if the remaining bytecount is
	     * within 16 of a multiple of 512 we round it off.  Can anyone
	     * tell me how the overrun can be stopped?
	     */
	    if (p->u_datasz >= 512 && ((xfer_cnt + 16) & 0x1FF) < 16)
	    {
		DPRINTF(0, ("scsi_intr: WARNING: DMA overrun\n"));
		xfer_cnt &= ~0x1FF;
		xfer_cnt += 0x200;
	    }
	}
	/* Total bytes transferred = p->u_datasz - xfer_cnt */
	p->u_data += p->u_datasz - xfer_cnt;
	p->u_datasz = xfer_cnt;
	break;
    }

    assert(!(getl(&p->u_flags) & SF_BUSY));

    /* Otherwise we have to judge things by bus phase */
    switch (status & NCR_BP_MASK)
    {
    case NCR_BP_DATAO:
    case NCR_BP_DATAI:
	DPRINTF(0, ("scsi_intr: ctlr %d in DATA phase\n", ctlr));
	if (c->c_state == STATE_XFERING_INFO)
	{
	    /* We are at the end of the DMA */
	    DPRINTF(0, ("scsi_intr: ctlr %d DATA xfer completed ok\n", ctlr));

	    if (p->u_datasz != 0)
	    {
		DPRINTF(-1, ("scsi_intr: DMA done but still %d bytes to go!\n",
								p->u_datasz));
		/* Should we reset the bus?? */
		p->u_errstat = STD_SYSERR;
		enqueue(scsi_bus_reset, ctlr);
		enqueue(command_done, (long) p);
		return;
	    }

	    /* Now acknowledge the last byte */
	    c->c_state = STATE_PHASE_CTL;
	    DPRINTF(0, ("end data phase ack\n"));
	    putb(&sbc->w.sc_cmd, NCR_CMD_MSG_ACCEPTED);
	}
	else
	{
	    /* We should start the pending DMA */
	    DPRINTF(0, ("scsi_intr: start data transfer\n"));
	    if (getl(&p->u_flags) & SF_WRITE)
		assert((status & NCR_BP_MASK) == NCR_BP_DATAO);
	    else
		assert((status & NCR_BP_MASK) == NCR_BP_DATAI);
	    start_xfer_data(c);
	}
	break;

    case NCR_BP_CMD:
	DPRINTF(0, ("scsi_intr: ctlr %d in CMD phase\n", ctlr));
	FLUSH_FIFO(sbc);
	PUT_CMD_IN_FIFO(sbc, p);
	scsi_dma_enable_intr(ADR_DMACTLR(ctlr));
	c->c_state = STATE_PHASE_CTL;
	putb(&sbc->w.sc_cmd, NCR_CMD_XFER_INFO);
	break;

    case NCR_BP_STATUS:
	DPRINTF(0, ("scsi_intr: ctlr %d in status phase\n", ctlr));
	/* Start command complete sequence */
	FLUSH_FIFO(sbc);
	c->c_state = STATE_CMD_CMPLT_SEQ;
	putb(&sbc->w.sc_cmd, NCR_CMD_ICMD_CMPLT_SEQ);
	break;

    case NCR_BP_MESGI:
	DPRINTF(0, ("scsi_intr: ctlr %d in MESG-IN phase\n", ctlr));
	if (c->c_state != STATE_GETMESG)
	{
	    /* We have to get the message */
	    FLUSH_FIFO(sbc);
	    c->c_state = STATE_GETMESG;
	    putb(&sbc->w.sc_cmd, NCR_CMD_XFER_INFO);
	    break;
	}
    
	/* Otherwise a message byte has been fetched! */
	if (intr_stat != NCR_ISTAT_FUNC_CMPLT)
	{
	    printf("scsi_intr: ctlr %d GET_MESG failed\n", ctlr);
	    p->u_errstat = STD_SYSERR;
	    enqueue(command_done, (long) p);
	}
	else
	    analyze_mesg_in(sbc, c, p);
	break;

    case NCR_BP_MESGO:
	DPRINTF(0, ("scsi_intr: ctlr %d in MESG-OUT phase to unit %d\n",
							    ctlr, p->u_unit));
	c->c_state = STATE_PHASE_CTL;
	putb(&sbc->w.sc_cmd, NCR_CMD_XFER_INFO);
	break;
    }
}


/*
 * scsi_intr
 *	The low level scsi interrupt handler.  This is where all the action
 *	happens since everything is interrupt driven.
 */

static void
scsi_intr(vecinfo)
int     vecinfo;
{
    ncr_539X *	sbc;
    scsi_ctlr *	c;
    int32	ctlr;
    int		intr_stat;
    int		status;
    int		seq_step;
    int		found;

    /* Figure out who generated the interrupt. Maybe there was more than one */
    found = 0;
    for (ctlr = 0, c = Scsi_ctlr_tab;
				     ctlr < (int32) Num_scsi_ctlrs; c++, ctlr++)
    {
	if (c->c_vecinfo != vecinfo)
	    continue; /* can't be it! */

	sbc = ADR539X(ctlr);

	/* See who interrupted & acknowledge the interrupt if it is them */
	status = getb(&sbc->r.sc_stat) & 0xFF;
	seq_step = getb(&sbc->r.sc_seqstep) & NCR_SEQUENCE_STEP;
	intr_stat = getb(&sbc->r.sc_intr_stat) & 0xFF;

	/* Make sure there was no DMA error */
	if (scsi_dma_error(ADR_DMACTLR(ctlr)))
	{
	    DPRINTF(-1, ("scsi_intr: scsi_dma_error on ctlr %d\n", ctlr));
	    c->c_current->u_errstat = STD_SYSERR;
	    enqueue(command_done, (long) c->c_current);
	    return;
	}

	/* Did this SCSI controller interrupt? */
	if (INTR_PENDING(intr_stat))
	{
	    handle_intr(c, ctlr, sbc, seq_step, status, intr_stat);
	    found++;
	}
    }
    if (!found)
	printf("scsi_intr: invalid vector (%x)\n", vecinfo);
}


static int
intr_pending(c, ctlr, sbc)
scsi_ctlr *	c;
int32		ctlr;
ncr_539X *	sbc;
{
    int		intr_stat;
    int		status;
    int		seq_step;

    disable();
    status = getb(&sbc->r.sc_stat) & 0xFF;
    seq_step = getb(&sbc->r.sc_seqstep) & NCR_SEQUENCE_STEP;
    intr_stat = getb(&sbc->r.sc_intr_stat) & 0xFF;
    DPRINTF(0, ("intr_pending: s=%02x ss=%x is=%02x, st=%x\n",
			status, seq_step, intr_stat, c->c_state));
    if (INTR_PENDING(intr_stat))
    {
	scsi_unlock(c);
	handle_intr(c, ctlr, sbc, seq_step, status, intr_stat);
	enable();
	threadswitch(); /* Call the scheduler */
	return 1;
    }
    return 0;
}


/****************************************/
/*	External Interface Routines	*/
/****************************************/

/*
 * scsi_cmd
 *	Starts up a SCSI command.  With this chip the irq_flag is somewhat
 *	irrelevant - you can only do interrupt driven I/O with this chip.
 *	That being the case it also makes little sense to do polled I/O
 *	and not do DMA since the DMA chip is tightly coupled to the SCSI
 *	chip and gets in the way.  Therefore the irq_flag is largely ignored
 *	except to decide whether to disconnect or not.
 *
 *	If datasz is 0 then it is assumed that there is no I/O to be done.
 *	If datasz is non-zero and data is the NULL pointer then there has been
 *	a programming error.
 *
 *	The way we work is basically to send out the relevant select command
 *	and then in the c_state field we remember which command we sent last.
 *	We need this to analyze the interrupts.  There is also a special state
 *	STATE_DISCONNECTED for when the initiator is not busy sending a command.
 *	After the select has been sent everything happens at interrupt level.
 */

void
scsi_cmd(ctlr, unit, irq_flag)
int32	ctlr;           /* scsi controller number */
int	unit;           /* target & logical unit to send command to */
int	irq_flag;       /* 0 = do command in polled mode, 1 = interrupt mode */
{
    ncr_539X *	sbc;
    scsi_unit *	p;
    scsi_ctlr *	c;
    int		target;
    uint8	cmd;
    vir_bytes	l_data;
    vir_bytes	l_datasz;

    c = &Scsi_ctlr_tab[ctlr];
    p = c->c_unit[unit];
    target = unit >> 3;

    DPRINTF(0, ("scsi_cmd: (%d, %d, %d)\n", ctlr, unit, irq_flag));
    DPRINTF(0, ("scsi_cmd: dp=0x%x, dsz=0x%x, w=%x, csz=%x\n",
	    p->u_data, p->u_datasz, (p->u_flags & SF_WRITE)?1:0, p->u_cmdsz));
    if (target < 0 || target >= 8 || p == 0 || p->u_datasz < 0 ||
		(p->u_datasz != 0 && p->u_data == 0) ||
		(p->u_datasz > MAX_DMA_BYTES) ||
		/* DMAs from memory must be 4 byte aligned */
		((p->u_flags & SF_WRITE) && (((long) p->u_data) & 3)) ||
		(p->u_cmdsz != 6 && p->u_cmdsz != 10 && p->u_cmdsz != 12))
    {
	p->u_errstat = STD_ARGBAD;
	DPRINTF(0, ("scsi_cmd: return 1"));
	return;
    }

    DPRINTF(0, ("scsi_cmd: u=%02d, cmd = %x\n",
					unit, p->u_cmd.c_6.command & 0xff));
    /*
     * We are going to modify the hardware registers, use global data
     * structures, so we lock the ctlr.
     * However a reslect might have already happened so we look at the state
     * and if it is not disconnected then a reselect is pending and we
     * give way to it.
     */
    sbc = ADR539X(ctlr);

    for (;;)
    {
	{
	    interval	delay;
	    interval	curmilli;

	    /*
	     * See if it is safe to access the bus - wait until time >= lbolt.
	     */
	    curmilli = getmilli();
	    if ((delay = c->c_lbolt - curmilli) > 0)
	    {
		DPRINTF(1, ("scsi_cmd: waiting for lbolt (%d)\n", unit));
		(void) await((event) 0, delay);
	    }
	    else
	    {
		/* avoid timer wrap problems */
		c->c_lbolt = curmilli;
	    }
	}

	p->u_errstat = STD_OK; /* until proven otherwise! */
	(void) scsi_lock(c, 0);

	/*
	 * Disable interrupts so that pending reselects can't be interfered
	 * with by our select command.  We must do the select in its entirety
	 * or we will get problems.
	 */

	disable();

	/* Check that a reselect is not already in progress.  */
	if (c->c_state != STATE_DISCONNECTED)
	{
	    DPRINTF(2, ("scsi_cmd: preempted by reselect\n"));
	    enable();
	    threadswitch(); /* Call the scheduler */
	    continue; /* RETRY */
	}

	/*
	 * When we flush the cache we need the addresses to flush.  We save
	 * them now since p->u_data* change during the course of the command.
	 */
	l_data = (vir_bytes) p->u_data;
	l_datasz = (vir_bytes) p->u_datasz;

	/*
	 * Arbitrate, select and send the command unless we need synchronous
	 * negotiation.  Although a reselect could be occurring at this point
	 * our tinkering with the fifo, etc will cause no harm since the
	 * flush-fifo and select commands will be ignored and we won't overfill
	 * the fifo to the point that the reselect messages are lost.  We never
	 * put more than 13 bytes into it at this point.
	 */

	FLUSH_FIFO(sbc);

	/* Set timeout register, Select ID & Initiator ID */

	DPRINTF(0, ("scsi_cmd: unit = %02d, synch period = %d, offset = %d\n",
					    unit, p->u_syn_tp, p->u_syn_off));

	/* On the 539X's you must set an extra bit for fast SCSI */
	if (getl(&p->u_flags) & SF_FASTSCSI)
	    NCR539X_SETFAST(sbc, c->c_type);
	else
	    NCR539X_CLRFAST(sbc, c->c_type);

	putb(&sbc->w.sc_sync_offset, p->u_syn_off);
	putb(&sbc->w.sc_sync_period, p->u_syn_tp & NCR_STP_SYNC_TP);

	putb(&sbc->w.sc_selbusid, (char) target);

	/* Unit p is now the subject of the controller's attention */
	c->c_current = p;

	if (getl(&p->u_flags) & SF_CAN_MESGS)
	{
	    uint8	ident_mesg;

	    ident_mesg = (unit & SCM_LUNIT_MASK);
	    if (irq_flag == IRQ_DISCONNECT)
		ident_mesg |= SC_MESS_IDEN | SCM_DISCON;
	    else
		ident_mesg |= SC_MESS_IDEN;
	    putb(&sbc->w.sc_fifo, (char) ident_mesg);

	    if (getl(&p->u_flags) & SF_NEGSYNC)
		cmd = NCR_CMD_SEL_ATN_STOP;
	    else /* Normal case */
		cmd = NCR_CMD_SEL_ATN;
	}
	else
	    cmd = NCR_CMD_SEL_NOATN;

	if (cmd != NCR_CMD_SEL_ATN_STOP)
	{
	    PUT_CMD_IN_FIFO(sbc, p);
	}

	scsi_dma_enable_intr(ADR_DMACTLR(ctlr));

	c->c_state = STATE_SELECT;
	put_OR_l(&p->u_flags, SF_SELECTING);

	/* And finally start the command rolling! */
	putb(&sbc->w.sc_cmd, (char) cmd);
	(void) getb(&sbc->r.sc_cmd);  /* read back the command register? */

	enable();

	/* If we were preempted then we retry */
	if (p->u_errstat == SERR_RESELECT_PENDING)
	{
	    put_AND_l(&p->u_flags, ~SF_DONE);
	    threadswitch();
	    continue; /* RETRY */
	}

	/*
	 * Wait for an interrupt now to signal completion of the command.
	 * If the command is already finished we don't wait.
	 */
	if (!(getl(&p->u_flags) & SF_DONE) &&
		    await((event) p, (interval) p->u_timeout) != 0)
	{
	    if (getl(&p->u_flags) & SF_SELECTING)
	    {
		enable(); 
		threadswitch(); /* Run the interrupt handling */
		printf("scsi_cmd: targ %d didn't get a select intr\n", target);
		continue;
	    }
	    if (getl(&p->u_flags) & SF_DONE)
	    {
		DPRINTF(0, ("scsi_cmd: lost a wakeup call!\n"));
	    }
	    else
	    {
		int tmp = c->c_state;

		/* Let's try to get the interrupt to come through */
		putb(&sbc->w.sc_cmd, NCR_CMD_SEL_ENABLE);
		enable(); 
		printf("scsi_cmd: ctlr %d: cmd for target %d troublesome\n",
								ctlr, target);
		threadswitch(); /* Run the interrupt handling */
		if (c->c_state == tmp)
		{
		    printf("scsi_cmd: enabling interrupts again didn't help\n");
		    if (intr_pending(c, ctlr, sbc))
		    {
			printf("hmmm\n");
			await((event) p, (interval) p->u_timeout);
		    }
		}
		if (!(getl(&p->u_flags) & SF_DONE))
		{
		    volatile scsi_unit * q;

		    printf("scsi_cmd: ctlr %d: cmd for target %d hanging in state %d\n",
							    ctlr, target, c->c_state);
		    printf("scsi_cmd: dataptr = %x, bytes remaining = %x\n",
								p->u_data, p->u_datasz);
		    printf("scsi_cmd: dma regs: stat = %x, addr = %x, byte_cnt = %x\n",
				ADR_DMACTLR(ctlr)->l_stat_ctl,
				ADR_DMACTLR(ctlr)->l_addr, 
				ADR_DMACTLR(ctlr)->l_byte_cnt);
		    p->u_errstat = STD_SYSERR;
		    put_AND_l(&p->u_flags, ~SF_BUSY);
		    for (q = c->c_discon; q != 0; q = q->u_nxtdcon)
		    {
			printf("unit %d is disconnected\n", q->u_unit);
		    }
		    if (!(getl(&p->u_flags) & SF_DISCON))
		    {
			DPRINTF(2,
			("scsi_cmd: unit %d still had the ctlr after timeout\n",
									unit));
			scsi_bus_reset(ctlr);
			scsi_unlock(c);
			return;
		    }
		}
	    }
	}

	if (p->u_errstat == SERR_RESELECT_PENDING)
	{
	    put_AND_l(&p->u_flags, ~SF_DONE);
	    threadswitch();
	    continue; /* RETRY */
	}

	break; /* If we get here all is well */
    }

    /* We shouldn't have the lock on the controller. If so we tidy up. */
    if (!(getl(&p->u_flags) & SF_DISCON))
    {
	/* We still have the lock on the controller */
	c->c_state = STATE_DISCONNECTED;
	putb(&sbc->w.sc_cmd, NCR_CMD_SEL_ENABLE);
	scsi_unlock(c);
    }
    else
	put_AND_l(&p->u_flags, ~SF_DISCON);
    put_AND_l(&p->u_flags, ~SF_DONE);
    DPRINTF(0, ("scsi_cmd: ctlr %d, unit %d done (err=%d)\n",
						ctlr, unit, p->u_errstat));
    /*
     * Now that the command has executed we flush the cache.  We can't safely
     * do it while the DMA is running, unfortunately.  On the Sun4m its only a
     * few instructions, so it's no big deal.
     * We only need to flush on a read since the SPARCstations have
     * write-through caches.
     */
    if (l_data != 0 && !(getl(&p->u_flags) & SF_WRITE))
	cache_flush(l_data, l_datasz);
    return;
}


/*
 * scsi_ctlr_init
 *      Initialise SCSI Controller
 */

/*ARGSUSED*/
errstat
scsi_ctlr_init(devaddr, ivec)
long	devaddr;
int	ivec;	/* unused */
{
    scsi_ctlr *	c;
    int32	ctlr;

    /* See if we already know about this controller */
    for (c = Scsi_ctlr_tab, ctlr = 0; ctlr < Num_scsi_ctlrs; c++, ctlr++)
    {
	if (c->c_addr == devaddr)
	{
	    /* We found our controller */
	    break;
	}
    }

    DPRINTF(0, ("scsi_ctlr_init: %d %d\n", ctlr, ivec));

    if (ctlr >= Num_scsi_ctlrs)
    {
	/* We have a new controller! */
	if (++Num_scsi_ctlrs > SUN_NUM_SCSI_CONTROLLERS)
	{
	    DPRINTF(0, ("scsi_ctlr_init: found %d ctlrs - only support %d\n",
			    Num_scsi_ctlrs, SUN_NUM_SCSI_CONTROLLERS));
	    return STD_NOTFOUND;
	}
	c->c_addr = devaddr;
    }

    if (!c->c_initted)	/* in the kernel this can't be preempted */
    {
	if (Scsi_info[ctlr].s_dma_ctlr == 0)
	    return STD_NOTFOUND;
	Sd_Lim_blk_cnt = MAX_DMA_BYTES / 512;
	scsi_initlock(c);
	c->c_discon = (scsi_unit *) 0;
	c->c_lbolt = 0;
	c->c_lmioff = 0;
	c->c_lmibuf = (uint8 *) malloc((size_t) SCSI_MAX_MESG_SIZE);
	if (c->c_lmibuf == 0)
	{
	    printf("scsi_ctlr_init: malloc failed for ctlr %d\n", ctlr);
	    return STD_NOMEM;
	}
	c->c_vecinfo = setvec(Scsi_info[ctlr].s_ivec, scsi_intr);
	DPRINTF(0, ("scsi_ctlr_init: vecinfo = %x\n", c->c_vecinfo));

	/* Mask the clock conversion factor to the right number of bits */
	CLOCK_CONV_FACTOR(ctlr) &= NCR_CLK_CNV_FACTOR;
	assert(CLOCK_CONV_FACTOR(ctlr) != 1);
	c->c_initted = 1;
    }

    (void) scsi_lock(c, 0);

    /*
     * If we have done a reset within the RESET_TIMEOUT then no other commands
     * have occurred and we can skip the extra reset.  Otherwise we do a
     * reset.
     */
    if (Scsi_ctlr_tab[ctlr].c_lbolt <= getmilli())
    {
	/*
	 * Initialize the DMA chip, including enabling interrupts since
	 * The interrupt pin for the DMA chip also enables the SCSI chip
	 */
	scsi_dma_reset(ADR_DMACTLR(ctlr));

	/*
	 * And now we can init the SCSI chip, wait for a bit and then
	 * clear the reset condition
	 */
	ncr539X_reset(ctlr);
	check_type(ctlr); /* Find out chip-type */
	if (c->c_type == CHIP_NCR539X)
	    c->c_minperiod = NCR_STP_MIN_PERIOD_9X;
	else
	    c->c_minperiod = NCR_STP_MIN_PERIOD_90;
	/*
	 * We can't actually manage synchronous SCSI with the NCR 5390 (alias
	 * the ESP 100).  These appear in SPARCstation 1 machines.  Therefore
	 * we need to know that up front.  Therefore we figure it out in the
	 * initialisation routines and record it here.
	 */
	if (c->c_type == CHIP_NCR5390)
	    c->c_max_so = 0;
	else
	    c->c_max_so = NCR_SO_MAX_OFFSET;

	DPRINTF(0, ("scsi_ctlr_init: ctlr %d: min sp = %d, max so = %d\n",
				ctlr, c->c_minperiod, c->c_max_so));

	ncr539X_reset(ctlr);
	scsi_bus_reset(ctlr);
	await((event) 0, (interval) 1000);
    }

    c->c_state = STATE_DISCONNECTED;

    scsi_unlock(c);
    return STD_OK;
}
