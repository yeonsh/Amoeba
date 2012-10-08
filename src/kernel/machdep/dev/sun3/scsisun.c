/*	@(#)scsisun.c	1.9	96/02/27 13:53:10 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *  scsisun.c
 *
 *	This file contains the source code for the sun3 scsi interface.
 *	The interface consists of the ncr5380 SCSI controller and the
 *	am9516a DMA controller.
 *	This version does not allow for chaining of commands but does
 *	support disconnect/reconnect for commands that are not polled.
 *	POLLED or SENSE commands are interrupt driven but disconnects
 *	are not allowed in these cases.
 *
 *	It provides the interface as required by the generic sd.c and st.c
 *	scsi disk and tape interfaces.
 *	There are two external identifiers:
 *
 *	scsi_cmd       - execute a command on the scsi bus and wait till it
 *			 comletes.  Must set p->u_errstat and p->u_datasz
 *			 so that at completion the latter reflects the number
 *			 of bytes *not* transferred.
 *	scsi_ctlr_init - set the vector for a scsi device and
 *			 initialise the device and scsi bus.
 *
 *  Author:
 *	Greg Sharp (Sept 1990 - Jan 1991)
 *	- derived from an earlier version by Leo van Moergestel, heavy
 *	  reading of the scsi standard and the ncr5380 & am9516a data sheets.
 *  Modified:
 *	Kees Verstoep (May 1991)
 *	- support for ANSI C volatile construction. Allows -O flag on ANSI
 *	  compiler to be used.
 *	Greg Sharp (Oct/Nov 1991)
 *	- added disconnect/reconnect support - the code isn't very pretty
 *	  but there is little point in a major tidyup since the Sun3 is
 *	  already obselete.
 *	Ed Keizer (March 1992)
 *	- there seemed to be a reason for making obsolete work. It seems
 *	  to work now. I had to cater for a horrid hardware bug and remove
 *	  a few software bugs.
 *      Greg Sharp (Aug 1995)
 *	- after a request sense no EOP interrupt occurs so the size of the
 *	  sense data remains unknown.  To fix this I added dma_stop which
 *	  for now is called where previously the DMA was stopped.
 */


#include "amoeba.h"
#include "assert.h"
INIT_ASSERT
#include "cmdreg.h"
#include "stderr.h"
#include "machdep.h"
#include "memaccess.h"
#include "map.h"
#include "scsi.h"
#include "scsi_sun3.h"
#include "ncr5380.h"
#include "am9516a.h"
#include "disk/disk.h"
#include "debug.h"
#include "sys/proto.h"
#include "arch_proto.h"


#ifdef SDEBUG
struct {
	int	p1 ;
	int	in_int ;
	int	flag ;
} d_scsi ;
#endif

/*
 * An implicit one-to-one relationship exists between the DMA controller
 * and the SCSI controller.  The two are separated so that the generic
 * SCSI code knows nothing of the DMA controller.  In this case we don't
 * need to use the locks for the DMA controller.  The SCSI one suffices
 * for both arrays.
 * In addition, for each controller we have the device address information
 * in the array Scsi_info defined in conf.c.
 */
scsi_ctlr			Scsi_ctlr_tab[SUN_NUM_SCSI_CONTROLLERS];
dma_ctlr			Dma_ctlr_tab[SUN_NUM_SCSI_CONTROLLERS];
unsigned long			Num_scsi_ctlrs;

/* True if DMA needs to be stopped. */
static int			dma_running[SUN_NUM_SCSI_CONTROLLERS];

extern	struct scsi_info	Scsi_info[];

/*
 * Due to the ncr5380 not doing all we could desire, we need to be able
 * to do microsecond grain timeouts.  Alas, we must busy wait.
 * Each iteration of the loop is expected to take ~ 0.5 us and we count
 * the initial assignment to __n by not using >=0 but >0 in the while loop.
 * This is sun 3/60, sun 3/50 specific.  The timing is a little rough but
 * gives us the required delays for various events, such as arbitration.
 */
#define	MICROSEC_DELAY(t) \
	{ \
	    register volatile int __n = (t) << 1;	\
	    while (--__n > 0);				\
	}


/*
 * Some commonly performed actions on the ncr5380 scsi bus controller.
 * The parameter 's' is a pointer to the registers of the controller.
 */
#define putb(a, b)	mem_put_byte((volatile char *) (a), b)
#define getb(a)		mem_get_byte((volatile char *) (a))

#define	DISABLE_MON_BUSY(s)	putb(&(s)->w.sc_mode, \
				 getb(&(s)->r.sc_mode) & ~NCR_MON_BUSY)
#define	ENABLE_MON_BUSY(s)	putb(&(s)->w.sc_mode, \
				 getb(&(s)->r.sc_mode) | NCR_MON_BUSY)
#define	DISABLE_RESEL_INTR(s)	putb(&(s)->w.sc_selenb, 0)
#define	ENABLE_RESEL_INTR(s)	putb(&(s)->w.sc_selenb, INITIATOR_ID)

#define	NCR5380_DMA_OFF(s)	putb(&(s)->w.sc_mode, \
				 getb(&(s)->r.sc_mode) & ~NCR_DMA_MODE)
#define	NCR5380_DMA_ON(s)	putb(&(s)->w.sc_mode, \
				 getb(&(s)->r.sc_mode) | NCR_DMA_MODE)

#define	NCR5380_INTR_CLEAR(s) \
	do {							\
	    register uint8 dummy = getb(&(s)->r.sc_reset);	\
	} while (0)

#define	MISSED_PHASE_INTR(s)	\
	((getb(&(s)->r.sc_cbstat) & NCR_CBSR_REQ) &&	\
	 !(getb(&(s)->r.sc_bas) & NCR_IRQ_ACTIVE))

/* For sun hardware */
#define	SUN_FIFO_DIR(s, direction) \
	do {						\
	    if (direction == SS_SEND)			\
		mem_OR_short((short *) &(s)->ss_stat, SS_SEND);	\
	    else					\
		mem_AND_short((short *) &(s)->ss_stat, ~SS_SEND);	\
	} while (0)

/* The state info necessary to catch a reselect interrupt on time! */
#define	STATE_RESELECTED	1
#define	STATE_NORESELECT	0


/********************************/
/*	Locking Code		*/
/********************************/

/*
 * We can't use mutexes here since we need a mechanism of unlocking such that
 * you must run to you get a lock.  Mutexes assign a lock to a new process
 * at unlock time and don't wait for the process to start running
 */

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


/* This unlock routine cannot be called from interrupt level */
static void
scsi_unlock(c)
scsi_ctlr *	c;
{
    c->c_mtx = 0;
    wakeup((event) c);
}


/* This unlock routine can be called from interrupt level */
static void
scsi_intr_unlock(c)
scsi_ctlr *     c;
{
    if (c->c_mtx == 1)
    {
        c->c_mtx = 0;
	enqueue((void (*)())wakeup, (long) c);
    }
}


/****************************************/
/*	DMA controller (UDC) routines	*/
/****************************************/

static void
udc_write_reg(ctlr, pointer, value)
int32	ctlr;
uint16	pointer;
uint16	value;
{
    register am_9516a * dma = ADR9516(ctlr);

    mem_put_short((short *) &dma->udc_raddr, (short) pointer);
    MICROSEC_DELAY(UDC_ACCESS_DELAY);
    mem_put_short((short *) &dma->udc_rdata, (short) value);
    MICROSEC_DELAY(UDC_ACCESS_DELAY);
}


/********************************/
/*	Sun SCSI Front End	*/
/********************************/

static void
sunscsi_reset(ctlr, cmd)
int32	ctlr;
uint16	cmd;
{
    sun_scsi *	sunsc = ADRSUNSCSI(ctlr);

    mem_AND_short((short *) &sunsc->ss_stat, (short) (~cmd));
    mem_OR_short((short *) &sunsc->ss_stat, (short) cmd);
}


static void
scsi_int_on(ctlr)
int32	ctlr;
{
    sun_scsi *	sunsc = ADRSUNSCSI(ctlr);

    mem_OR_short((short *) &sunsc->ss_stat, SS_SBC_INT_EN);
}


static void
scsi_int_off(ctlr)
int32	ctlr;
{
    sun_scsi *	sunsc = ADRSUNSCSI(ctlr);

    mem_AND_short((short *) &sunsc->ss_stat, ~SS_SBC_INT_EN);
}


/************************/
/*	SCSI routines	*/
/************************/

static void
clear_mode_reg(ctlr)
int32	ctlr;
{
    register ncr_5380 *	sc = ADR5380(ctlr);

    putb(&sc->w.sc_mode, 0);
}


/*
 * Reset the SCSI bus
 */

static void
scsi_bus_reset(ctlr)
int32	ctlr;
{
    register ncr_5380 *	 sbc = ADR5380(ctlr);
    register sun_scsi *	 sunsc = ADRSUNSCSI(ctlr);
    scsi_ctlr *		 c = &Scsi_ctlr_tab[ctlr];
    volatile scsi_unit * p;

    DPRINTF(0, ("Resetting SCSI bus of controller %d\n", ctlr));

    /* We disable interrupts from the scsi controller */
    mem_AND_short((short *) &sunsc->ss_stat, ~SS_SBC_INT_EN);

    /* Assert the reset signal for the minimum period */
    putb(&sbc->w.sc_icom, NCR_ASSERT_RESET);
    MICROSEC_DELAY(SC_MIN_RESET);
    putb(&sbc->w.sc_icom, 0);

    /* Clear any pending interrupts or errors */
    NCR5380_INTR_CLEAR(sbc);

    /* Let the poor devices have a few seconds to recover - especially tapes */
    c->c_lbolt = getmilli() + 4000;

    /* Go through table and mark all disconnected jobs as "forgotten" */
    for (p = c->c_discon; p != 0; p = p->u_nxtdcon)
    {
	if (p->u_devtype == INQ_SEQ_ACCESS)
	    p->u_errstat = STD_SYSERR; /* the tape was probably rewound! */
	else
	    p->u_errstat = SERR_AGAIN;
	p->u_flags &= ~SF_DISCON;
	wakeup((event) p);
    }
    c->c_discon = 0;
    /* Unlock the controller in case it was locked */
    scsi_intr_unlock(c);
}


/*
 * When we want to reset all devices - udc, SCSI bus & Sun fifo interface
 */

static
last_resort(ctlr)
int32	ctlr;
{
    printf("scsi: last resort - resetting SCSI bus\n");
    reset_all(ctlr);
}

reset_all(ctlr)
int32	ctlr;
{
    /* Reset the DMA controller */
    udc_write_reg(ctlr, PTR_COMMAND, CR_RESET);
    udc_write_reg(ctlr, PTR_MASTERMODE, MM_INIT);

    /* Reset the sun interface */
    sunscsi_reset(ctlr, SS_SCSI_RESET);
    sunscsi_reset(ctlr, SS_FIFO_RESET);

    /* Reset the SCSI bus */
    scsi_bus_reset(ctlr);
}


/*
 * Set SCSI bus phase
 *	returns -1 for illegal phase,
 *	returns 0 for phases which can be tested
 *	returns 1 for phases which can't be tested
 */

static int
set_phase(sc, fase)
register ncr_5380 *	sc;
int			fase;
{
    switch (fase)
    {
    case SBP_DATAO:
	sc->w.sc_tcom = 0;
	break;
    case SBP_DATAI:
	sc->w.sc_tcom = NCR_TCOM_IO;
	break;
    case SBP_CMD:
	sc->w.sc_tcom = NCR_TCOM_CD;
	break;
    case SBP_STATUS:
	sc->w.sc_tcom = NCR_TCOM_CD | NCR_TCOM_IO;
	break;
    case SBP_MESSO:
	sc->w.sc_tcom = NCR_TCOM_CD | NCR_TCOM_MSG;
	break;
    case SBP_MESSI:
	sc->w.sc_tcom = NCR_TCOM_CD | NCR_TCOM_MSG | NCR_TCOM_IO;
	break;
    case SBP_FREE:
    case SBP_SELECT:
    case SBP_RESELECT:
    case SBP_ARBIT:
	sc->w.sc_tcom = 0;
	return 1;
    case SBP_UNSPECIFIED: /* for when we want a phase change */
	sc->w.sc_tcom = NCR_TCOM_MSG;
	break;
    default:
	return -1;
    }
    return 0;
}


/*
 * Set and test SCSI bus Phase
 */

static
set_test_phase(ctlr, fase)
int32	ctlr;
int	fase;
{
    register ncr_5380 *	sbc = ADR5380(ctlr);

    switch (set_phase(sbc, fase))
    {
    case -1:	/* illegal phase */
    default:
	return NOT_OK;

    case 0:	/* Test if requested phase == expected phase */
    	if (!(getb(&sbc->r.sc_bas) & NCR_PHASE_MATCH))
	    return DIFFPH;
	/* return OK; we can fall through and save a little code */

    case 1:	/* Untestable phase */
	return OK;
    }
}


/*
 * Free SCSI bus
 *	Turn off monitoring of bus busy.
 *	Clear any errors and pending interrupts
 *	Drive the bus to the Bus Free state
 */

static void
scsi_free(ctlr)
int32	ctlr;
{
    register ncr_5380 *	sbc = ADR5380(ctlr);

    DISABLE_MON_BUSY(sbc);
    NCR5380_INTR_CLEAR(sbc);
    putb(&sbc->w.sc_icom, 0);
    (void) set_phase(sbc, SBP_FREE);
}


/*
 *	DMA Controller Code
 *
 *	dma_setup - setup the scsi chip and the dma chip for a transfer
 *	dma_go	  - start the dma operation
 */

static void
dma_setup(p)
scsi_unit *	p;	/* scsi unit to do the dma with */
{
    register sun_scsi *	sunsc = ADRSUNSCSI(p->u_ctlr);
    dma_cmdblk *	dma_cmd = &Dma_ctlr_tab[p->u_ctlr].d_cmd;
    long		ptr;
    int			direction;
    long		bufp;

    if (p->u_datasz <= 0)
    {
	p->u_flags &= ~SF_DMA_WANTED;
	return;
    }
    p->u_flags |= SF_DMA_WANTED;

    if (p->u_flags & SF_WRITE)
    {
	direction = SS_SEND;
	dma_cmd->d_regsel = UDC_RSEL_SEND;
	dma_cmd->d_lcmr = UDC_CMR_LSEND;
    }
    else
    {
	direction = SS_RECEIVE;
	dma_cmd->d_regsel = UDC_RSEL_RECEIVE;
	dma_cmd->d_lcmr = UDC_CMR_LRECV;
    }

    /* Adjust for DVMA offset if nessecary */
    bufp = (long) p->u_data;
    if (bufp < DVMA_OFFSET(p->u_ctlr))
	bufp += DVMA_OFFSET(p->u_ctlr);

    /* Setup dma load table */
    dma_cmd->d_haddr = (int16) (((bufp & 0xFF0000) >> 8) | UDC_ADDR_CTRL);
    dma_cmd->d_laddr = bufp & 0xFFFF;
    dma_cmd->d_cnt = p->u_datasz >> 1;	/* convert byte-count to word-count */
    dma_cmd->d_hcmr = UDC_CMR_HIGH;

    ptr = (long) dma_cmd;

    /* Soft reset dma controller chip */
    udc_write_reg(p->u_ctlr, PTR_COMMAND, CR_RESET);

    /* Reset the fifo */
    sunscsi_reset(p->u_ctlr, SS_FIFO_RESET);
    /* Set up fifo direction and count */
    SUN_FIFO_DIR(sunsc, direction);
    mem_put_short((short *) &sunsc->ss_cnt, (short) p->u_datasz);

    /* Soft reset dma controller chip */
    udc_write_reg(p->u_ctlr, PTR_COMMAND, CR_RESET);

    /* Reset the fifo */
    sunscsi_reset(p->u_ctlr, SS_FIFO_RESET);

    udc_write_reg(p->u_ctlr, PTR_CHAR_HIGH, (uint16) ((ptr & 0xFF0000) >> 8));
    udc_write_reg(p->u_ctlr, PTR_CHAR_LOW, (uint16) (ptr & 0xFFFF));
    udc_write_reg(p->u_ctlr, PTR_MASTERMODE, MM_INIT);
    udc_write_reg(p->u_ctlr, PTR_COMMAND, UDC_CMD_C1ENABLE);
}


static errstat
dma_go(p)
scsi_unit *	p;
{
    register ncr_5380 *	sc = ADR5380(p->u_ctlr);
    register sun_scsi *	sunsc = ADRSUNSCSI(p->u_ctlr);

    mem_OR_short((short *) &sunsc->ss_stat, SS_SBC_INT_EN);

	DPRINTF(4, ("Status 0x%x, residue %d, data_sz %d\n",
		    mem_get_short((short *) &sunsc->ss_stat),
		    mem_get_short((short *) &sunsc->ss_cnt),
		    p->u_datasz));
    /* start the dma chip */
    udc_write_reg(p->u_ctlr, PTR_COMMAND, UDC_CMD_START_CHAIN);

    /* now tell the scsi chip that the dma is coming */
    if (p->u_flags & SF_WRITE)
    {
	DPRINTF(2, ("SCSI: dma_go: write\n"));
	disable();
	if (set_test_phase(p->u_ctlr, SBP_DATAO) == DIFFPH)
	{
	    printf("scsi_dma: unexpected bus phase requested\n");
	    enable() ;
	    NCR5380_DMA_ON(sc);
	    return SERR_BADPHASE;
	}
	putb(&sc->w.sc_icom, NCR_ASSERT_DBUS);
	NCR5380_DMA_ON(sc);
	/* start initiator send */
	putb(&sc->w.sc_sdsend, 0);
	enable() ;
    }
    else
    {
	DPRINTF(2, ("SCSI: dma_go: read\n"));
	if (set_test_phase(p->u_ctlr, SBP_DATAI) == DIFFPH)
	{
	    printf("scsi_dma: unexpected bus phase requested\n");
	    NCR5380_DMA_ON(sc);
	    return SERR_BADPHASE;
	}
	NCR5380_DMA_ON(sc);
	/* start initiator receive */
	putb(&sc->w.sc_sdirec, 0);
    }
    dma_running[p->u_ctlr] = 1;
    return STD_OK;
}


static void
dma_stop(ctlr)
int32	ctlr;
{
    int32	remainder;
    volatile scsi_unit * p = Scsi_ctlr_tab[ctlr].c_current;
    register ncr_5380 *	sc = ADR5380(p->u_ctlr);

    NCR5380_DMA_OFF(sc);
    if (dma_running[ctlr])
    {
	remainder = (int32) mem_get_short((short *) &ADRSUNSCSI(ctlr)->ss_cnt);
	p->u_data += p->u_datasz - remainder;
	p->u_datasz = remainder;
	dma_running[ctlr] = 0;
    }
    NCR5380_DMA_ON(sc); /* reenable interrupts, just in case they're needed */
}


/*
 * Do SCSI bus arbitration and selection
 *	We follow the flow chart of the ncr5380 chip in combination with the
 *	SCSI standard sections 5.1.2 and 5.1.3.
 *  NB. We assume that the monitoring of the busy signal is off.  After all,
 *	arbitration is supposed to wait for the bus to be free.
 */

static errstat
scsi_arbit_select(sc, targetid, disconflag)
register ncr_5380 *	sc;
int8			targetid;	/* target to select */
int			disconflag;	/* true if disconnect should happen */
{
    int			select_timer;
    errstat		error;

    /* Make sure that arbitration bit is off */
    putb(&sc->w.sc_mode, getb(&sc->r.sc_mode) & ~NCR_ARBITRATE);

    /*
     * Start arbitration phase -
     *   make sure we aren't asserting any bus signals
     *   NB: when we set the arbitrate bit the chip seems to wait for bus-free
     *   write our scsi id on the data bus
     *   tell the chip to commence arbitration.
     */
    putb(&sc->w.sc_tcom, 0);
    putb(&sc->w.sc_icom, 0);
    putb(&sc->w.sc_data, INITIATOR_ID);
    putb(&sc->w.sc_mode, getb(&sc->r.sc_mode) | NCR_ARBITRATE);

    /*
     * Test arbitration in progress bit.
     * In principle we should loop forever here but it seems that someone might
     * try to reselect us and AIP may never be asserted until we deal with it.
     * Therefore we wait for at most NCR_AIP_WAIT microseconds and if it hasn't
     * started then we give up.
     */
    if (!(getb(&sc->r.sc_icom) & NCR_AIP))
    {
	int	count = NCR_AIP_WAIT;

	do
	{
	    MICROSEC_DELAY(1);
	    if (--count <= 0)
	    {
		putb(&sc->w.sc_mode,
			getb(&sc->r.sc_mode) & ~NCR_ARBITRATE);
		/* should check to see if we are being reselected */
		if ((getb(&sc->r.sc_cbstat) & NCR_CBSR_RESEL) == NCR_CBSR_RESEL
			    && (getb(&sc->r.sc_data) & INITIATOR_ID))
		{
		    return SERR_RESELECT_PENDING;
		}
		putb(&sc->w.sc_icom, 0);
		return SERR_LOSTARB;
	    }
	} while (!(getb(&sc->r.sc_icom) & NCR_AIP));
    }

    MICROSEC_DELAY(SC_ARBIT_DELAY);
    /*
     *  Check result of arbitration (LA-bit in sc.icom).
     *  OR maybe another device is slow in asserting SEL on the bus.
     *  OR check one more time for losing arbitration.
     * NB: We assume/pray that the chip does the right things to the bus when
     *     the arbitration bit is switched off.
     */
    if ((getb(&sc->r.sc_icom) & NCR_LOST_ARB) ||
	((getb(&sc->r.sc_data) & ~INITIATOR_ID) > INITIATOR_ID) ||
	(getb(&sc->r.sc_icom) & NCR_LOST_ARB))
    {
	putb(&sc->w.sc_mode, getb(&sc->r.sc_mode) & ~NCR_ARBITRATE);
	putb(&sc->w.sc_icom, 0);
	return SERR_LOSTARB;
    }

    /*
     * We arrive here if no other higher priority device wants the bus.
     * There can be a lower priority device but that should give up the bus.
     * Therefore we assert select and busy and wait for a bus clear and a bus
     * settle delay.  (This is 1.2 us and so we can actually leave it out
     * since the putting the targetid in the register takes longer than that.)
     */
    putb(&sc->w.sc_icom, (getb(&sc->r.sc_icom) & 0x1F) | NCR_ASSERT_SEL);
#if 0
    MICROSEC_DELAY(SC_BUS_CLEAR_DELAY + SC_BUS_SETTLE_DELAY);
#endif

    /*
     *	Selection Phase (see sections 5.1.3 & 5.2.1 of the SCSI Standard)
     */
    putb(&sc->w.sc_data, targetid | INITIATOR_ID);

    /* Assert select, busy and data bus, plus atn if we want to disconnect */
    if (disconflag)
    {
	putb(&sc->w.sc_icom,
	 (getb(&sc->r.sc_icom) & 0x1F) | NCR_ASSERT_DBUS | NCR_ASSERT_ATN);
    }
    else
    {
	putb(&sc->w.sc_icom, (getb(&sc->r.sc_icom) & 0x1F) | NCR_ASSERT_DBUS);
    }
    
    /* Reset arbitration bit */
    putb(&sc->w.sc_mode, getb(&sc->r.sc_mode) & ~NCR_ARBITRATE);

    /* Release busy */
    putb(&sc->w.sc_icom, (getb(&sc->r.sc_icom) & 0x1F) & ~NCR_ASSERT_BSY);

    /* Look for assertion of busy by the selected target */
#if 0
    /* This delay is unnecessary - we just go around the loop a few times
     * until busy is asserted which is ok since NCR_SEL_TIMEOUT allows for it
     */
    MICROSEC_DELAY(SC_BUS_SETTLE_DELAY);
#endif
    error = STD_OK;
    select_timer = NCR_SEL_TIMEOUT;
    while (--select_timer)
	if (getb(&sc->r.sc_cbstat) & NCR_CBSR_BSY)
	    break;

    /*
     * If we got a select timeout OR busy was de-asserted while we were tidying
     * up we take option 2 (see section 5.1.3.5 of the standard).  We don't
     * reset the bus (option 1) since there might be disconnected operations
     * that would be killed by the reset.
     */
    if (!(getb(&sc->r.sc_cbstat) & NCR_CBSR_BSY))
    {
	putb(&sc->w.sc_icom, (getb(&sc->r.sc_icom) & 0x1F) & ~NCR_ASSERT_DBUS);
	select_timer = SC_SEL_ABORT_TIME;
	while (--select_timer)
	{
	    if (getb(&sc->r.sc_cbstat) & NCR_CBSR_BSY)
		break;
	    MICROSEC_DELAY(10);
	}
	error = SERR_SELFAIL;
    }

    /*
     * Whether we succeeded or not we release select and the data bus
     * If the target isn't driving busy then the select failed and the bus
     * goes into the free state.
     */
    putb(&sc->w.sc_icom, (getb(&sc->r.sc_icom) & 0x1F) &
					~(NCR_ASSERT_SEL | NCR_ASSERT_DBUS));

    if (!(getb(&sc->r.sc_cbstat) & NCR_CBSR_BSY))
	error = SERR_SELFAIL;

    return error;
}


/*
 * Write 'cnt' bytes on the scsi bus
 */

static errstat
write_bytes(ctlr, ptr, cnt)
int32		ctlr;
register char *	ptr;
register int	cnt;
{
    register ncr_5380 *	sc = ADR5380(ctlr);
    int			timer;

    while (--cnt >= 0)
    {
	/* Make sure ACK and the data bus are not asserted */
	putb(&sc->w.sc_icom, 0);
 
	/* Wait for REQ signal on SCSI bus */
	timer = RW_BYTE_TIMEOUT;
	while (!(getb(&sc->r.sc_cbstat) & NCR_CBSR_REQ))
	    if (--timer < 0)
		return SERR_PHASEWAIT;

	putb(&sc->w.sc_data, *ptr++);
	putb(&sc->w.sc_icom, NCR_ASSERT_DBUS);
	/* Need a 55 ns pause between asserting the data bus and asserting ack
	*/
	mem_OR_byte((char *) &sc->w.sc_icom, NCR_ASSERT_ACK);

	/* Wait for REQ to go false before releasing ACK */
	timer = RW_BYTE_TIMEOUT;
	while (getb(&sc->r.sc_cbstat) & NCR_CBSR_REQ)
	    if (--timer < 0)
	    {
		putb(&sc->w.sc_icom, 0);
		return SERR_PHASEWAIT;
	    }
    }
    putb(&sc->w.sc_tcom, NCR_TCOM_MSG);
    NCR5380_INTR_CLEAR(sc);
    /* De-assert ACK, ATN and the data bus */
    putb(&sc->w.sc_icom, 0);
    return STD_OK;
}


/*
 * The initiator sends a message to the target
 */

static errstat
mesg_out(ctlr, ptr)
int32	ctlr;
char *	ptr;	/* pointer to the message to send */
{
    int	cnt;

    if (set_test_phase(ctlr, SBP_MESSO) != OK)
	return SERR_BADPHASE;

    /* Figure out the size of the message */
    if (*ptr != SC_MESS_EXT)
	cnt = 1;
    else
	/*
	 * We are sending an extended message.
	 * Get length adjust for length byte.
	 */
	if ((cnt = (*(ptr+1) & 0xff) + 2) == 2)
	    cnt = 258;

    /* Send bytes (see section 5.1.5.1 of the standard) */
    return write_bytes(ctlr, ptr, cnt);
}


/*
 * get current SCSI bus phase
 */

static int
get_phase(ctlr)
int32	ctlr;
{
    int			status;
    register ncr_5380 *	sc = ADR5380(ctlr);

    status = getb(&sc->r.sc_cbstat);

    if (!(status & NCR_CBSR_REQ))	/* No valid phase on SCSI bus */
	return SBP_UNSPECIFIED;

    /* Get MSG, IO and CD bits */
    status &= NCR_CBSR_MSG | NCR_CBSR_IO | NCR_CBSR_CD;

    switch (status)
    {
    case NCR_PHASE_DATA_OUT:
	return SBP_DATAO;

    case NCR_PHASE_DATA_IN:
	return SBP_DATAI;

    case NCR_PHASE_CMD:
	return SBP_CMD;

    case NCR_PHASE_STATUS:
	return SBP_STATUS;

    case NCR_PHASE_MESG_OUT:
	return SBP_MESSO;

    case NCR_PHASE_MESG_IN:
	return SBP_MESSI;

    default:
	return SBP_UNSPECIFIED;
    }
}


/*
 * ack_mesg
 *  This routine is needed at disconnection and reconnection to acknowledge
 *  a message *after* we have performed what the message required.  This is
 *  necessary to avoid race conditions.  This code is very similar to that
 *  at the end of read_byte!
 */

static void
ack_mesg(sc)
register ncr_5380 *	sc;	/* scsi bus controller */
{
    char	icom;
    int		timer;

    /* Get rid of top 3 bits for safety & assert ACK */
    icom = getb(&sc->r.sc_icom) & ~(NCR_ASSERT_RESET | NCR_AIP | NCR_LOST_ARB);
    putb(&sc->w.sc_icom, icom | NCR_ASSERT_ACK);

    /* Wait for REQ to go false before releasing ACK */
    timer = RW_BYTE_TIMEOUT;
    while (getb(&sc->r.sc_cbstat) & NCR_CBSR_REQ)
	if (--timer < 0)
	    break;

    /* Release ACK by restoring original bus state */
    putb(&sc->w.sc_icom, icom);
}


/* Called at disconnects or command complete's */

static void
scsi_end(ctlr)
int32	ctlr;
{
    register ncr_5380 *	sc = ADR5380(ctlr);
    scsi_ctlr *	c = &Scsi_ctlr_tab[ctlr];

    NCR5380_DMA_OFF(sc);
    DISABLE_MON_BUSY(sc);
    /* Just to be sure we kill off any pending interrupts */
    NCR5380_INTR_CLEAR(sc);
    ENABLE_RESEL_INTR(sc);
    (void) set_phase(sc, SBP_FREE);
    ack_mesg(sc);
    /* Free the SCSI bus */
    putb(&sc->w.sc_icom, 0);
    /* Unlock Controller */
    scsi_unlock(c);
}


/*
 *	Read a data byte from SCSI target into *p
 */

static errstat
read_byte(ctlr, p, phase)
int32	ctlr;
char *	p;
int	phase;
{
    register ncr_5380 *	sc = ADR5380(ctlr);
    int			timer;
    int8		icom;

    /* Wait for ASSERT REQUEST signal on SCSI bus */
    timer = RW_BYTE_TIMEOUT;
    while (!(getb(&sc->r.sc_cbstat) & NCR_CBSR_REQ))
	if (--timer < 0)
	{
	    putb(&sc->w.sc_icom, 0);
	    return SERR_PHASEWAIT;
	}

    /* Make sure that bus still has correct phase */
    if (get_phase(ctlr) != phase)
	return SERR_BADPHASE;

    /* Receive byte */
    *p = getb(&sc->r.sc_data);

    /* Get rid of top 3 bits for safety & assert ACK */
    icom = getb(&sc->r.sc_icom) & ~(NCR_ASSERT_RESET | NCR_AIP | NCR_LOST_ARB);
    putb(&sc->w.sc_icom, icom | NCR_ASSERT_ACK);

    /* Wait for REQ to go false before releasing ACK */
    timer = RW_BYTE_TIMEOUT;
    while (getb(&sc->r.sc_cbstat) & NCR_CBSR_REQ)
	if (--timer < 0)
	    return SERR_PHASEWAIT;
    if (phase == SBP_MESSI && *p == SC_MESS_CC)
    {
	putb(&sc->w.sc_tcom, NCR_TCOM_MSG);
	NCR5380_DMA_OFF(sc);
	NCR5380_INTR_CLEAR(sc);
    }
    /* Release ACK by restoring original bus state */
    putb(&sc->w.sc_icom, icom);
    return STD_OK;
}


/*
 * The initiator receives a message from the target
 */

static errstat
mesg_in(ctlr, ptr)
int32	ctlr;
char *	ptr;
{
    register int	cnt;
    errstat		err;

    /* Adjust target command register to expected values */
    if (set_test_phase(ctlr, SBP_MESSI) != OK)
	return SERR_BADPHASE;

    if ((err = read_byte(ctlr, ptr, SBP_MESSI)) != STD_OK)
	return err;

    if (*ptr != SC_MESS_EXT)
	return STD_OK;

    /* We are receiving an extended message - get message byte count value */
    ptr++;
    if ((err = read_byte(ctlr, ptr, SBP_MESSI)) != STD_OK)
        return err;
    if ((cnt = *ptr++) == 0)
	cnt = 256;

    /* Get the extended message bytes */
    while (--cnt >= 0)
    {
	if ((err = read_byte(ctlr, ptr, SBP_MESSI)) != STD_OK)
	    return err;
	ptr++;
    }
    return STD_OK;
}


/*
 * Busy wait for SCSI bus phase to match one of those specified here.
 * Note that this routine can only be used to wait for the following
 * bus phases:
 *	Data Out, Data In, Command, Status, Message Out, Message In.
 * This is because we first wait for REQ to be asserted before examining
 * the phase.  This avoids spurious phase matches.
 */
 
static int
wait_phase(ctlr, fase1, fase2, fase3, fase4)
int32	ctlr;
int     fase1;
int     fase2;
int     fase3;
int     fase4;
{
    int	fase;
    int	phasetimer = PHASE_TIMEOUT;

    while (--phasetimer)
	if ((fase = get_phase(ctlr)) == fase1 || fase == fase2 ||
					fase == fase3 || fase == fase4)
		return fase;

    return SBP_TIMEOUT;
}


/*
 * scsi_send_cmd actually puts the command onto the bus for the target.
 * It assumes that the bus is already in command phase.  It also assumes
 * that interrupts are disabled for the duration since there is some
 * timing critical stuff happening here.
 */

static errstat
scsi_send_cmd(p)
scsi_unit *	p;		/* pointer to command block to send */
{
    register ncr_5380 *	sbc;
    register int	nbytes;
    int			timer;
    char *		cp;	/* pointer to command block */

    /* We should be in command phase.  Let's check for old time's sake */
    if (set_test_phase(p->u_ctlr, SBP_CMD) != OK)
	return SERR_PHASELOST;

    switch (nbytes = p->u_cmdsz)
    {
    case 6:
	cp = (char *) &p->u_cmd.c_6;
	break;
    case 10:
	cp = (char *) &p->u_cmd.c_10;
	break;
    case 12:
	cp = (char *) &p->u_cmd.c_12;
	break;
    default:
	/* In theory this can't happen since we checked in scsi_cmd already */
	return STD_ARGBAD;
    }

    sbc = ADR5380(p->u_ctlr);
    putb(&sbc->w.sc_tcom, NCR_TCOM_CD);
    while (--nbytes >= 0)
    {
	/*
	 * Prepare to put info on the data bus.  Round the loop this also
	 * de-asserts ACK for the previously written byte.
	 */
	putb(&sbc->w.sc_icom, NCR_ASSERT_DBUS);

	/* Wait for REQUEST signal on SCSI bus */
	timer = RW_BYTE_TIMEOUT;
	while (!(getb(&sbc->r.sc_cbstat) & NCR_CBSR_REQ))
	    if (--timer < 0)
		return SERR_CMDFAIL;

	/* Put command byte on bus and assert acknowledge */
	putb(&sbc->w.sc_data, *cp++);
	putb(&sbc->w.sc_icom, getb(&sbc->r.sc_icom) | NCR_ASSERT_ACK);

	/* Wait for REQ to go false before releasing ACK */
	timer = RW_BYTE_TIMEOUT;
	while (getb(&sbc->r.sc_cbstat) & NCR_CBSR_REQ)
	    if (--timer < 0)
		return SERR_CMDFAIL;
    }
    NCR5380_INTR_CLEAR(sbc);
    putb(&sbc->w.sc_tcom, NCR_TCOM_MSG); /* unspecified bus phase! */
    NCR5380_DMA_ON(sbc);

    /* De-assert ACK for last byte put on bus */
    putb(&sbc->w.sc_icom, 0);
    return STD_OK;
}


/*
 * sync_phase
 *	This is called on phase interrupts to either start a pending
 *	DMA or to terminate a SCSI operation.  Note that we cannot
 *	call scsi_unlock since this is run at enqueued-interrupt level.
 */

static void
sync_phase(ctlr)
int32	ctlr;
{
    int		phase;
    char	mes_ptr[SCSI_MAX_MESG_SIZE];
    scsi_unit *	p;
    ncr_5380 *	sc = ADR5380(ctlr);
    scsi_ctlr *	c = &Scsi_ctlr_tab[ctlr];
    uint8	status;

    DPRINTF(3, ("SCSI: sync_phase %d\n", ctlr));
    p = (scsi_unit *) c->c_current;
    /*
     * We really should wait until REQ is asserted before proceeding!
     * Get_phase does check to be sure it is on but doesn't wait, so we
     * won't get a bogus phase.  Only really slow devices are likely
     * to bite us after an EOP.
     */
    switch (phase = get_phase(ctlr))
    {
    case SBP_DATAI:
    case SBP_DATAO:
	DPRINTF(2, ("sync_phase(%d, %d): starting DMA\n", ctlr, p->u_unit));
	if (p->u_flags & SF_DMA_WANTED)
	{
	    p->u_errstat = dma_go(p);
	    p->u_flags &= ~SF_DMA_WANTED;
	}
	else
	    (void) set_phase(sc, phase);
	break;

    case SBP_STATUS:
	/* Switch off any dma that may have been running */
	DPRINTF(2, ("SCSI: sync_phase: status\n"));
	NCR5380_DMA_OFF(sc); /* Make sure any DMA has stopped */
	(void) set_phase(sc, SBP_STATUS);
	status = getb(&sc->r.sc_data);
	(void) set_phase(sc, SBP_UNSPECIFIED);
	NCR5380_DMA_ON(sc);
	if (status != STAT_GOOD)
	{
	    if (status == STAT_CHECK_CON)
		p->u_errstat = SERR_CHECKCON;
	    else
		p->u_errstat = SERR_CMDFAIL;
	}
	/* Release the ACK for the read of the status byte */
	ack_mesg(sc);
	/* Now we should get a command complete message */
	break;

    case SBP_MESSI:
	/*
	 * We don't use mesg-in here since there is a race to perform the
	 * required operation(s) if we acknowledge the message before doing
	 * them.  Eg. we must disable busy monitoring before we ack the
	 * disconnect.  If it is not an expected message we can use mesg_in.
	 * To get the next phase interrupt we need to change the phase
	 * to something that will always be changed.
	 */
	mes_ptr[0] = getb(&sc->r.sc_data);
	switch (mes_ptr[0] & 0xff)
	{
	case SC_MESS_SDP:
	    /* Save Data Pointer(s) */
	    DPRINTF(2, ("SDP(%d, %d)\n", ctlr, p->u_unit));
	    /*
	     * The actual data offsets were saved in dma_stop.
	     * The next phase should also be mesg-in.
	     */
	    (void) set_phase(sc, SBP_UNSPECIFIED);
	    ack_mesg(sc);
	    break;

	case SC_MESS_RDP:
	    DPRINTF(2, ("RDP(%d, %d)\n", ctlr, p->u_unit));
	    /* Restore Data Pointer(s) */
	    (void) set_phase(sc, SBP_UNSPECIFIED);
	    ack_mesg(sc);
	    break;

	case SC_MESS_DIS:
	    /*
	     * Disconnect - mark it disconnected and release the controller.
	     * Any moment now the target will release busy so we had better
	     * stop monitoring it.
	     */
	    DPRINTF(2, ("Discon(%d, %d)\n", ctlr, p->u_unit));
	    p->u_flags |= SF_DISCON;
	    /* Put p on disconnect queue */
	    p->u_nxtdcon = c->c_discon;
	    c->c_discon = p;

	    /* Ack the message and let go of the SCSI bus */
	    scsi_end(ctlr) ;

	    break;
	
	case SC_MESS_CC:
	    /* The command is finished! */
	    DPRINTF(2, ("sync_phase(%d, %d): cmd complete\n", ctlr, p->u_unit));
	    /* Ack the message and let go of the SCSI bus */
	    scsi_end(ctlr) ;
#ifdef SDEBUG
	    {
		sun_scsi *	sunsc = ADRSUNSCSI(ctlr);
		int32		bytecount;

		bytecount = (int32) mem_get_short((short *) &sunsc->ss_cnt);
		if (bytecount)
		{
		    printf("CC: %d bytes remain\n", bytecount);
		}
	    }
	    if (p->u_datasz)
	    {
		printf("CC: %d bytes remain\n", p->u_datasz);
	    }
#endif
	    p->u_flags |= SF_BUSY;
	    wakeup((event) p); /* Wakeup the client */
	    break;

	default:
	    /* Identify messages are handled by reselect() */
	    (void) mesg_in(ctlr, mes_ptr);
	    printf("SCSI sync_phase: got unexpected message %x\n",
							    mes_ptr[0] & 0xff);
	    (void) set_phase(sc, SBP_UNSPECIFIED);
	    break;
	}
	break;

    case SBP_MESSO:
	DPRINTF(0, ("SCSI sync_phase: unexected message-out phase\n"));
	break;

    case SBP_CMD:
	DPRINTF(0, ("SCSI sync_phase: unexpected command phase\n"));
	(void) set_phase(sc, SBP_CMD);
	break;

    default:
	printf("synch_phase: unknown bus phase %d, cbstat 0x%x\n",
		phase, getb(&sc->r.sc_data));
#ifdef SDEBUG
	d_scsi.flag=1 ;
	printf("p1 %d, in_int %d\n", d_scsi.p1, d_scsi.in_int) ;
#endif
	break;
    }
#ifdef SDEBUG
d_scsi.p1 = 0 ;
#endif
}


/*
 * Handle the reselection: see section 5.1.4 in the Standard
 */

static void
reselect(ctlr)
int32	ctlr;
{
    ncr_5380 *		sc = ADR5380(ctlr);
    register int	datareg;
    register int	target;
    long		select_timer;
    int			phase;
    int			poll_flag;
    int			phasetimer = PHASE_TIMEOUT;
    int			bstat;
    scsi_ctlr *		c;

    /*
     * We must assert BSY within a selection abort time (200 ms)!!
     *
     * First we have to lock the controller.
     * This has to be FAST so it must not be locked!  We lock without retry.
     * Therefore a reselect interrupt should not be possible while someone
     * else has this lock!  Otherwise there is a BUG!
     */
    c = &Scsi_ctlr_tab[ctlr];
    if (scsi_lock(c, 1) == 0)
	panic("SCSI reselect: couldn't get controller %ld on reselect", ctlr);

    c->c_state = STATE_NORESELECT;
    /* Read target ID from the data bus and switch off our own ID bit */
    datareg = getb(&sc->r.sc_data) & 0xff;
    datareg &= ~INITIATOR_ID;

    /*
     * Must make sure that only one SCSI ID is on before acknowledging.
     * In two's complement arithmetic if x != 0, x == 2^n iff (x & (x-1)) == 0.
     */
    if (datareg == 0 || (datareg & (datareg - 1)) != 0)
    {
	/*
	 * We don't know who is waiting for this reselect so we can't
	 * wake them up; they just have to time out.
	 */
	printf("SCSI reselect: Ctlr %ld got more than one target id: %x\n",
								ctlr, datareg);
	ENABLE_RESEL_INTR(sc);
	/* Unlock controller */
	scsi_intr_unlock(c);
    }
    else
    {
	/* We have to set BSY 200 microsseconds after detecting SEL */
	disable();
	bstat = getb(&sc->r.sc_cbstat) ;
	if ( (bstat&(NCR_CBSR_SEL|NCR_CBSR_BSY|NCR_CBSR_IO))!=
		    (NCR_CBSR_SEL|NCR_CBSR_IO)) {
	    enable();
	    if ( ! (bstat&NCR_CBSR_SEL) ) {
		DPRINTF(0, ("scsi reselect: lost SEL\n")) ;
	    }
	    if ( bstat&NCR_CBSR_BSY ) {
		DPRINTF(0, ("scsi reselect: got BUSY\n")) ;
	    }
	    if ( ! (bstat&NCR_CBSR_IO) ) {
		DPRINTF(0, ("scsi reselect: lost IO\n")) ;
	    }
	    goto GIVE_UP;
	}
	/* Acknowledge reselection by driving busy */
	putb(&sc->w.sc_icom, (getb(&sc->r.sc_icom) & 0x1F) | NCR_ASSERT_BSY);
	enable();

	/* Work out which SCSI target is reselecting. */
	for (target = 0; target < 8 && datareg != 1; target++)
	    datareg >>= 1;
	DPRINTF(2, ("SCSI: reselect by target %d\n", target));

	/* Target should now drop select */
	select_timer = NCR_SEL_TIMEOUT;
	while (--select_timer)
	    if (!(getb(&sc->r.sc_cbstat) & NCR_CBSR_SEL))
		break;

	if (select_timer == 0) /* SEL never went off */
	{
GIVE_UP:    ENABLE_RESEL_INTR(sc);
	    /* Unlock controller */
	    scsi_intr_unlock(c);
	    printf("SCSI reselect: Ctlr %d: target %d protocol error\n",
								ctlr, target);
	}
	else
	{
	    /* Release BSY and let target go for it */
	    putb(&sc->w.sc_icom,
		    (getb(&sc->r.sc_icom) & 0x1F) & ~NCR_ASSERT_BSY);
	    ENABLE_MON_BUSY(sc);
	    /*
	     * Should get a phase interrupt now due to MesgIn phase of Target
	     * However the chip admits a race here so instead we poll for it.
	     */
	    phase = wait_phase(ctlr, SBP_MESSI,
				    SBP_NO_PHASE, SBP_NO_PHASE, SBP_NO_PHASE);
	    /* unspecified bus phase! */
	    if (phase != SBP_MESSI)
	    {
		ENABLE_RESEL_INTR(sc);
		/* Unlock controller */
		scsi_intr_unlock(c);
		printf("SCSI reselect: Ctlr %d: no mesg-in phase\n", ctlr);
	    }
	    else
	    {
		char mess;

		/*
		 * We cheat and see if it is an identify message. It should be.
		 */
		if ((mess = getb(&sc->r.sc_data)) & SC_MESS_IDEN) {
		    int unit;
		    scsi_unit * p;
		    volatile scsi_unit * q;

		    /*
		     * Can't read identify message using mesg_in.  We must do
		     * the dma setup before the data phase or we will miss
		     * the phase interrupt as it goes to data phase.  So first
		     * we have to find out the unit, set up its dma and then
		     * ACK the identify mesg.
		     * After that phase interrupts should get us home ...
		     */
		    unit = (target << 3) | (mess & 07);
		    p = c->c_unit[unit];
		    if (!(p->u_flags & SF_DISCON))
		    {
			printf("SCSI reselect: Controller %d: Got reselect from target %d, unit %d\nbut device wasn't disconnected\n", unit, target, mess & 07);
		    }
		    else
		    {
			/* Restore current pointer! */
			c->c_current = p;
			/* Remove p from disconnect list */
			q = c->c_discon;
			if (q == p)
			    c->c_discon = p->u_nxtdcon;
			else
			{
			    while (q != 0 && q->u_nxtdcon != p)
				q = q->u_nxtdcon;
			    assert(q != 0);
			    q->u_nxtdcon = p->u_nxtdcon;
			}
			p->u_flags &= ~SF_DISCON;

			/* If there is still I/O to do, set it up */
			dma_setup(p);

			/* Force undefined bus phase */
			(void) set_phase(sc, SBP_UNSPECIFIED);
			/* We try to prevent the horrid case mentioned
			   in int5380().
			 */
			poll_flag=0 ;
			if (p->u_flags & SF_DMA_WANTED)
			{
			    if (p->u_flags & SF_WRITE)
			    {
				poll_flag=1 ;
			    }
			}
			if ( !poll_flag ) {
			    NCR5380_DMA_ON(sc);
			}
			scsi_int_off(ctlr);
			ack_mesg(sc);
			/* Under normal circumstances we can return
			   and wait for the phase interupt to occur.
			   If we expect the DATA_OUT phase we have to poll.
			   The 5380 chip got itself into RESET while
			   trying to get the first byte from the fifo.
			 */
			if ( poll_flag ) {
			    DPRINTF(3, ("Polling for next phase\n")) ;
			    /* All this leaves poll_flag to 1 when
			       when we have seen phase change */
			    do {
				phase=get_phase(ctlr);
			    } while (phase==SBP_UNSPECIFIED && --phasetimer>0 ) ;
			    if ( phase==SBP_UNSPECIFIED ) {
				printf("SCSI reselect: waiting for phase\n") ;
				NCR5380_DMA_ON(sc);
				if ( get_phase(ctlr)!=SBP_UNSPECIFIED ) {
					/* We lost the race */
				} else {
					poll_flag=0 ;
				}
			    }
			}
			DPRINTF(3, ("Phase change to %d\n", phase));
			if ( poll_flag ) {
			    /* We now have phase change */
#ifdef SDEBUG
d_scsi.p1 = 1 ;
{ int bs ; bs=getb(&sc->r.sc_cbstat) ;
  if ( d_scsi.flag ) printf("flag error\n") ;
#endif
			    NCR5380_INTR_CLEAR(sc);
			    scsi_int_on(ctlr) ;
			    sync_phase(ctlr);
			    if ( !(phase==SBP_DATAO || phase==SBP_DATAI) ) {
				NCR5380_DMA_ON(sc);
			    }
#ifdef SDEBUG
d_scsi.p1 = 2 ;
if ( d_scsi.flag ) {
	printf("resel: bstat, before 0x%x, after 0x%x\n",
		bs, getb(&sc->r.sc_cbstat)) ;
	d_scsi.flag=0 ;
    }
}
#endif
			} else {
			    scsi_int_on(ctlr) ;
			}
		    }
		}
		else
		{
		    printf("SCSI reselect: no identify message!\n");
		    last_resort(ctlr);
		    /* Unlock controller */
		    scsi_intr_unlock(c);
		}
	    }
	}
    }
}


/* 
 * Low Level interrupt routine for NCR 5380.  This one just figures out
 * what sort of interrupt it was.
 *
 * Bits in bus and status register and current bus status
 *
 * NB. It seems like the documentation swapped the bus and status register
 *     states for parity error and reset.
 *
 *				    B&S Reg		     CBS Reg
 * EOP				1 0 0 1 0 0 0 X		0 1 X X X X 0 X
 * PARITY ERROR			0 X 1 1 1 0 X X		0 1 1 X X X 0 X
 * LOSS OF BUSY			0 0 0 1 X 1 0 0		0 0 0 X X X 0 0
 * SELECT/RESELECT		0 0 0 1 X 0 X 0		0 0 0 X X R 1 X
 * BUS PHASE MISMATCH		0 0 0 1 0 0 X 0		0 1 X X X X 0 X
 * RESET			0 X 0 1 X 0 X X		X X X X X X X X
 *  (The R means that it is reselect if 0 and select if 1)
 */

static int
int5380(ctlr, c)
int32		ctlr;
scsi_ctlr *	c;
{
    register ncr_5380 *	sbc = ADR5380(ctlr);
    uint8		bas;
    uint8		cb;

    /* Look in bus and status register for the reason for the interrupt */
    /* get rid of all don't care bits */
    bas = getb(&sbc->r.sc_bas);
    cb = getb(&sbc->r.sc_cbstat);

    /* Acknowledge interrupt */
    NCR5380_INTR_CLEAR(sbc);

    switch (bas&BAS_MASK)
    {
    case BAS_INT_EOP:
	if (c->c_current->u_flags & SF_WRITE)
	{
	    /* De-assert ACK for last byte put on bus */
	    putb(&sbc->w.sc_icom, 0);
	}
	if ((cb & CBS_MASK_EOP) == CBS_INT_EOP)
	    return NCR_INT_EOP;
	else
	    return NCR_INT_ERR;

    case BAS_INT_PAR:
	/* parity error interrupt */
	if ((cb & CBS_MASK_PAR) == CBS_INT_PAR)
	    return NCR_INT_PARITY;
	else
	    return NCR_INT_ERR;

    case BAS_INT_BUSY:
	if ((cb & CBS_MASK_BUSY) == CBS_INT_BUSY)
	    return NCR_INT_BUSY;
	else
	    return NCR_INT_ERR;

    default:
	/* There are 3 cases left: look at Current SCSI bus Status Register */
	if ((cb & CBS_MASK_SEL) == CBS_INT_SEL)
	{
	    /* Stop the reselect from happening again until we are ready */
	    DISABLE_RESEL_INTR(sbc);
	    NCR5380_INTR_CLEAR(sbc);
	    if (cb & NCR_CBSR_IO)
		return NCR_INT_RESEL;	/* reselect */
	    else
		return NCR_INT_SEL;	/* select interrupt */
	}

	/*
	 * Check for Bus Phase Mismatch.  If it is a mismatch interrupt then
	 * we check the three phase bits in the cbsr against those in the
	 * target command register to see if the bus phase really is different
	 */
	if ((cb & CBS_MASK_PHASE) == CBS_INT_PHASE)
	    if (((cb >> 2) & 7) == (getb(&sbc->r.sc_tcom) & 7))
		/* We should not get here */
		return NCR_INT_ERR;
	    else
		return NCR_INT_PHASE;
	else {
	/* If none of the above then it should be a reset interrupt */
	/* But once in a while the following happens:
	   We expect, and do the dma_setup because the bcr of the FIFO
	   needs to be written before the change to a DATA phase.
	   Now when that phase is DATA OUT, the 5380 tries to prefetch,
	   that has to fail, because the FIFO is empty. The phase change
	   interrupt to DATA OUT should then occur, which triggers this
	   driver to start the DMA chip.
	   The interrupt occurs alright, but ....
	   the SBC hardware around the SCSI and DMA chipped is locked
	   because the prefetch still has not finished yet. All 5380
	   registers read as 0xFF and all attempts to start the DMA fail.
	   We tried to prevent this circumstance in the code,
	   but you never know ....
	   All we can do is reset the SBC hardware, SCSI chip, DMA chip
	   Fifo etc. No attempt at a decent restart is currently made.
	 */
	    if ( bas== 0xFF && cb==0xFF ) {
		printf("int5380: that horrible case!!\n");
	    }
	    return NCR_INT_RESET;
	}
    }
}


/*
 * Low level scsi interrupt routine
 */
static void
scsi_intr(vecinfo)
int	vecinfo;
{
    register int32		ctlr;
    register scsi_ctlr *	c;
    int				reason;

#ifdef SDEBUG
    d_scsi.in_int=1 ;
#endif
    /* Figure out who generated the interrupt */
    for (ctlr = 0, c = Scsi_ctlr_tab; ; c++, ctlr++)
    {
	if (ctlr >= SUN_NUM_SCSI_CONTROLLERS)
	{
	    printf("scsi_intr: invalid vector (%d)\n", vecinfo);
	    return;
	}
	if (c->c_vecinfo == vecinfo)
	    break; /* found it! */
    }

    if (dma_running[ctlr])
	dma_stop(ctlr);

    /* Acknowledge the interrupt and determine the reason for it */
    switch(reason = int5380(ctlr, c))
    {
    case NCR_INT_ERR:
	DPRINTF(0, ("scsi_intr: got ERR interrupt\n"));
	last_resort(ctlr);
	break;

    case NCR_INT_RESEL:
	DPRINTF(12, ("scsi_intr: reselect\n"));
	/*
	 * We can't cope with more than one reselect at a time but int5380
	 * has disabled reselects. We set a flag to say we have been
	 * reselected.  This is cleared by the enqueued interrupt routine
	 * once the reselect has been handled.
	 */
	c->c_state = STATE_RESELECTED;
	enqueue(reselect, (long) ctlr);
	break;

    case NCR_INT_SEL:
	DPRINTF(0,
		("scsi_intr: unexpected select intr on controller %d\n", ctlr));
	last_resort(ctlr);
	break;

    case NCR_INT_EOP:
	{
	    DPRINTF(2, ("scsi_intr: EOP(%d, %d)\n", ctlr, c->c_current->u_unit));
	    enqueue(sync_phase, (long) ctlr);
	}
	break;

    case NCR_INT_RESET:
	DPRINTF(0, ("scsi_intr(%d): RESET interrupt\n", ctlr));
	last_resort(ctlr);
	break;

    case NCR_INT_PARITY:
	DPRINTF(0, ("scsi_intr: parity error on controller %d\n", ctlr));
	last_resort(ctlr);
	break;

    case NCR_INT_PHASE:
	DPRINTF(2, ("scsi_intr: phase\n"));
	enqueue(sync_phase, (long) ctlr);
	break;

    case NCR_INT_BUSY:
	DPRINTF(0, ("scsi_intr: lost busy signal\n"));
	last_resort(ctlr);
	break;

    default:
	printf("scsi_intr: received unknown interrupt type (%d)\n", reason);
	panic("scsi_intr: impossible interrupt");
	/*NOTREACHED*/
    }
#ifdef SDEBUG
    d_scsi.in_int=0 ;
#endif
}


/****************************************
 *	External Interface Routines	*
 ****************************************/

/*
 * scsi_ctlr_init
 *
 *	Initialise SCSI Controller
 *	The lock is never reinitialised since we must not break locks.
 *	The holders must clear them themselves.
 */

errstat
scsi_ctlr_init(devaddr, ivec)
long	devaddr;
int	ivec;	/* interrupt vector for this controller */
{
    long	ctlr;	/* the scsi controller */
    scsi_ctlr *	c;

    DPRINTF(0, ("scsi_ctlr_init: (0x%x, %d)\n", devaddr, ivec));

    /* See if we already know about this controller */
    for (c = Scsi_ctlr_tab, ctlr = 0; ctlr < Num_scsi_ctlrs; c++, ctlr++)
    {
	if (c->c_addr == devaddr)
	{
	    /* We found our controller */
	    break;
	}
    }

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

    if (!c->c_initted)  /* in the kernel this can't be preempted */
    {
	scsi_initlock(c);
	/* Set interrupt vector */
	c->c_vecinfo = setvec(ivec, scsi_intr);
	c->c_discon = (scsi_unit *) 0;
	c->c_lbolt = 0;
	c->c_initted = 1;
    }

    (void) scsi_lock(c, 0);
    /* hard reset udc, fifo, ncr5380 and soft reset scsi bus */
    reset_all(ctlr);
    clear_mode_reg(ctlr);

    scsi_unlock(c);
    return STD_OK;
}


/*
 * scsi_cmd
 *
 *	Starts up a SCSI command.  If the command is in polled mode it runs it
 *	to completion.  Otherwise it dozes off and waits for the interrupt.
 *	The interrupt routine wakes scsi_cmd up when the command is done and
 *	scsi_cmd returns the error status to the caller.
 *
 *	If datasz is 0 then it is assumed that there is no I/O to be done.
 *	If datasz is non-zero and data is the NULL pointer then there has been
 *	a programming error.  The data pointer must also be even byte aligned.
 */

void
scsi_cmd(ctlr, unit, irq_flag)
int32	ctlr;           /* scsi controller number */
int	unit;		/* target & logical unit to send command to */
int	irq_flag;	/* 0 = do command in polled mode, 1 = interrupt mode */
{
    register ncr_5380 *	sbc;
    scsi_unit *		p;
    scsi_ctlr *		c;
    int			target;
    int			discon;
    int			phase;
    int			i;
    int			timedout;

    c = &Scsi_ctlr_tab[ctlr];
    p = c->c_unit[unit];
    target = unit >> 3;

    DPRINTF(2, ("scsi_cmd: (%d, %d, %d)\n", ctlr, unit, irq_flag));
    if (target < 0 || target >= 8 || p == 0 || p->u_datasz < 0 ||
		(p->u_datasz != 0 && p->u_data == 0) ||
		(((long) p->u_data) & 1) ||
		(p->u_cmdsz != 6 && p->u_cmdsz != 10 && p->u_cmdsz != 12))
    {
	printf("scsi_cmd: invalid argument\n");
	p->u_errstat = STD_ARGBAD;
	return;
    }
    if ((p->u_flags & (SF_WRITE_PROT | SF_WRITE)) == (SF_WRITE_PROT | SF_WRITE))
    {
	p->u_errstat = STD_DENIED;
	return;
    }

    if (irq_flag == IRQ_DISCONNECT && (p->u_flags & SF_CAN_DISCON))
	discon = 1;
    else
	discon = 0;

    sbc = ADR5380(ctlr);
    p->u_errstat = STD_OK;	/* until proven otherwise! */

    /*
     * We are going to modify the hardware registers so we lock the controller.
     * We need SCSI interrupts disabled while we do arbitration and selection.
     * We detect reselection attempts that might happen while we are trying
     * to get the bus.
     */
RETRY:
    (void) scsi_lock(c, 0);
    DISABLE_RESEL_INTR(sbc);
    scsi_int_off(ctlr);
    DPRINTF(2, ("intr pending = %x\n", getb(&sbc->r.sc_bas) & 0x10));

    /* Unit p is currently the subject of the controller's attention */
    c->c_current = p;
    if (c->c_discon != 0) /* Then reselects might be pending */
    {
	DPRINTF(1, ("scsi_cmd: reselects might be pending\n"));
	if (c->c_state == STATE_RESELECTED)
	{
	    scsi_int_on(ctlr);
	    scsi_unlock(c);
	    DPRINTF(1, ("scsi_cmd: preempted by reselect\n"));
	    threadswitch(); /* Call the scheduler */
	    goto RETRY;
	}
    }
    DPRINTF(2, ("scsi_cmd: reselect interrupts disabled\n"));

    /*
     * See if it is safe to access the bus - must wait until the time is >=
     * lbolt.  We add one to the await time to avoid the race which would let
     * the timeout be 0 (ie. infinite).
     */
    if (getmilli() < c->c_lbolt)
    {
	DPRINTF(1, ("scsi_cmd: awaiting a lightning bolt\n"));
	(void) await((event) 0, (interval)(c->c_lbolt - getmilli() + 1));
    }

    /*
     *  One of the problems we have when selecting is that for a certain period
     *  after a bus reset, certain devices, especially those with slow media
     *  like tapes and optical disks, are often deaf to a select, sometimes for
     *  several seconds.  Therefore we retry the select a few times, to be sure.
     */
    i = 0;
    while (1)
    {
	disable();
	p->u_errstat = scsi_arbit_select(sbc, 1 << target, discon);
	enable();
	if (p->u_errstat == STD_OK)
	    break;
	if (p->u_errstat == SERR_RESELECT_PENDING)
	{
	    /* handle the reselect! */
	    scsi_int_on(ctlr);		/* we will need interrupts on */
	    scsi_unlock(c);
	    reselect(ctlr);		/* do the reselect */
	    /*
	     * Wait for the controller to come free again and then go
	     * back and try to arbitrate for the bus again
	     */
	    goto RETRY;
	}
	else
	{
	    if (p->u_errstat != SERR_SELFAIL && i < SELECT_RETRIES)
	    {
		(void) await((event) 0, (interval) 1000); /* wait 1 second */
		i++;
	    }
	    else /* could not select the device! */
	    {
		DPRINTF(0,
		    ("scsi_arbit_select failed for ctlr %ld, unit %d (%d)\n",
						    ctlr, unit, p->u_errstat));
		scsi_unlock(c);
		return;
	    }
	}
    }
    DPRINTF(2, ("scsi_cmd: selected\n"));

    NCR5380_DMA_OFF(sbc);
    ENABLE_MON_BUSY(sbc);

    /*
     * If discon is true then we set attention when selecting and want to enable
     * disconnects.  If we don't get a message-out phase from the target then we
     * reset.
     */
    if (discon)
    {
	phase = wait_phase(ctlr, SBP_MESSO,
				    SBP_NO_PHASE, SBP_NO_PHASE, SBP_NO_PHASE);
	if (phase == SBP_MESSO)
	{
	    /* Tell target we can do disconnects. */
	    char ident_mes;
		
	    ident_mes = SC_MESS_IDEN | (unit & SCM_LUNIT_MASK) | SCM_DISCON;
	    if (mesg_out(ctlr, &ident_mes) != STD_OK)
		discon = 0;
	}
	else
	{
	    p->u_errstat = SERR_BADPHASE;
	    scsi_bus_reset(ctlr);
	    scsi_unlock(c);
	    return;

	}
    }

    /*
     * Set up the dma registers for the transfer.  The phase change
     * interrupt after the command is sent causes the dma to be started
     * (if it goes into DATA-IN or DATA-OUT phase).  We do this now since
     * the command phase usually takes a while to begin and the Sun3 needs
     * the bcr to be set before the data I/O phase begins.
     */
    dma_setup(p);

    /*
     * Now we see what phase the bus moves on to.  If it fails to go to CMD
     * then something is not quite right.  In this case clean up the mess and
     * quit.
     */
    do
    {
	phase = wait_phase(ctlr, SBP_CMD, SBP_MESSI,
						SBP_NO_PHASE, SBP_NO_PHASE);
	switch (phase)
	{
	case SBP_MESSI:
	    {
		char	mes_ptr[SCSI_MAX_MESG_SIZE];
		/*
		 * Perhaps the target didn't like our identify and sent a reject
		 * message?
		 */
		(void) mesg_in(ctlr, mes_ptr);
		if (discon && (mes_ptr[0] & 0xff) == SC_MESS_REJ)
		    discon = 0; /* rejected the identify message! */
		else
		    DPRINTF(0, ("scsi_cmd: ctlr %d got unexpected mesg 0x%x\n",
						    ctlr, mes_ptr[0] & 0xff));
	    }
	    break;

	case SBP_CMD:
	    /* What we are waiting for. */
	    break;

	case SBP_TIMEOUT:
	    DPRINTF(0, ("scsi_cmd: stuck in phase %d\n", get_phase(ctlr)));
	    p->u_errstat = SERR_PHASEWAIT;
	    break;

	default:
	    printf("scsi_cmd: wait_phase returned %d\n", phase);
	    panic("scsi_cmd: impossible phase");
	    break;
	}
    } while (p->u_errstat == STD_OK && phase != SBP_CMD);

    if (p->u_errstat != STD_OK)
    {
	scsi_bus_reset(ctlr);
	scsi_unlock(c);
	return;
    }

    /* clear any phase mismatch interrupt due to the phase transitions */
    NCR5380_INTR_CLEAR(sbc);

    /*
     * If we got to here then we should be in the command phase.  We now send
     * the command to the target.  This is timing critical code so we shut out
     * interrupts while we do it.
     */
    disable();
    p->u_errstat = scsi_send_cmd(p);
    enable();
    if (p->u_errstat != STD_OK)
    {
	if (c->c_discon != 0)
	    ENABLE_RESEL_INTR(sbc);
	scsi_free(ctlr);
	scsi_unlock(c);
	return;
    }

    scsi_int_on(ctlr);

    /* check for lost phase change interrupt */
    if (MISSED_PHASE_INTR(sbc))
    {
	DPRINTF(0, ("scsi_cmd: missed phase intr\n"));
	scsi_intr(c->c_vecinfo);
    }
#ifdef SDEBUG
    DPRINTF(2, ("Await: mode 0x%x, cbstat 0x%x, bas 0x%x\n",
	getb(&sbc->r.sc_mode), getb(&sbc->r.sc_cbstat), getb(&sbc->r.sc_bas)));
#endif
    /*
     * We clear BUSY and let the command complete wakeup set it.
     * If we miss the wakeup we can detect that and report correct command
     * completion.
     */
    p->u_flags &= ~SF_BUSY;
    timedout = await((event) p, p->u_timeout);
    if (timedout != 0)
    {
	if (p->u_flags & SF_BUSY)
	{
	    DPRINTF(0, ("scsi_cmd: ctlr %d unit %d missed wakeup\n",
								ctlr, unit));
	}
	else
	{
#ifdef SDEBUG
    DPRINTF(2, ("mode 0x%x, cbstat 0x%x, bas 0x%x\n",
	getb(&sbc->r.sc_mode), getb(&sbc->r.sc_cbstat), getb(&sbc->r.sc_bas)));

	    { int bytecount ;
		sun_scsi *	sunsc = ADRSUNSCSI(ctlr);
		bytecount = (int32) mem_get_short((short *) &sunsc->ss_cnt);
		if ( bytecount ) {
		    printf("%d bytes remain\n", bytecount);
		}
	    }
#endif
	    reset_all(ctlr); /* also sets errstat for p! */
	    clear_mode_reg(ctlr);
	    printf("scsi_cmd: ctlr %d unit %d: command is hanging.\n",
								 ctlr, unit);
	}
    }
    p->u_flags &= ~SF_BUSY;

    /* Update admin in case of failure! */
    if (p->u_flags & SF_DISCON)
	p->u_flags &= ~SF_DISCON;
    return;

}
