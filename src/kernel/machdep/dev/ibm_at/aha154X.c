/*	@(#)aha154X.c	1.4	96/02/27 13:50:43 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *	Adaptec 154X SCSI Driver
 *
 *  This driver should work on the 1540, 1542A, 1542B, 1542C and 1542CF cards.
 *
 *  The generic SCSI command interface (see ../generic/s[dt].c) requires
 *  that the following entry points be defined in the low level driver:
 *
 *	scsi_cmd       - execute a command on the SCSI bus and
 *                       wait until it completes.
 *	scsi_ctlr_init - set the vector for a SCSI device and
 *                       initialise the device and SCSI bus.
 *	scsi_geom      - return the disk geometry for the specified unit.
 *			 This is a hack to cope with the 386 fdisk program.
 *
 * Authors:   Gregory J. Sharp & Kees J. Bot, Feb - Mar 1994
 * Modified:  Gregory J. Sharp & Kees J. Bot, Jan 1996 - if no device is
 *			 present then on some machines the in-byte returns 0
 *			 instead of 0xff.  Hence you could get false positives.
 */

#include "amoeba.h"
#include "stdlib.h"
#include "string.h"
#include "machdep.h"
#include "cmdreg.h"
#include "stderr.h"
#include "memaccess.h"
#include "map.h"
#include "scsi.h"
#include "sys/proto.h"
#include "debug.h"
#include "assert.h"
INIT_ASSERT
#include "aha154X.h"
#include "i386_proto.h"
#include "module/mutex.h"


extern errstat sd_capacity();

/* From the generic SCSI code */
extern	unsigned int	Sd_Lim_blk_cnt;

/* AHA-154x port addresses */
#define AHA_CNTLREG(c)	((int) ((c)+0))	/* Control Register - write only */
#define AHA_STATREG(c)	((int) ((c)+0))	/* Status Register - read only */
#define AHA_DATAREG(c)	((int) ((c)+1))	/* Data Register - read/write */
#define AHA_INTRREG(c)	((int) ((c)+2))	/* Interrupt Flags - read only */

/* Some limits to help cope with the 16 MB boundary */
#define	MAX_DMA_BYTES	(64*1024)	/* Largest I/O operation we attempt */
#define	LIM_16MB	0x1000000	/* Point above which we can't DMA */

/* Time to wait after a reset before trying to access a device */
#define	RESET_TIMEOUT	3000

/* Max number of mailboxes = max number of outstanding SCSI commands. */
#define	NR_MAILBOXES	8

#define	NR_CMDBLKS	8

/* All mailbox related variables. */
typedef struct cmd {
    ccb_t	ccb;		/* Command Control Block */
    phys_bytes	ccb_phys;	/* phys address of ccb */
    mutex	ccb_mtx;	/* Is it free? */
} cmd_t;


/* Information needed per controller */
typedef struct controller {
    mailbox_t	mailbox[2][NR_MAILBOXES]; /* out and in mailboxes */
    mailbox_t *	outptr;			  /* last out-mailbox used */
    char	buffer[MAX_DMA_BYTES];
    interval	lbolt;			  /* when the next command can begin */
    mutex	mtx;
} ctlr_t;


#define ccb_sense(pc)	((byte *) ((pc)->cmd + (pc)->cmdlen))

static ctlr_t		Ctlrs[MAX_SCSI_CONTROLLERS];
scsi_ctlr		Scsi_ctlr_tab[MAX_SCSI_CONTROLLERS];
unsigned int		Num_scsi_ctlrs;	    /* Total # controllers found */
cmd_t			Cmdblk[NR_CMDBLKS];
int			Cmbnext;


/*
 * The 386 is little-endian but the SCSI card does everything big-endian.
 * Therefore we have a few macros to do conversions for us.
 */

#define l2b16(b, l)		\
	do {			\
	    (b)[0] = (l) >> 8;	\
	    (b)[1] = (l);	\
	} while (0)

#define l2b24(b, l)		\
	do {			\
	    (b)[0] = (l) >> 16;	\
	    (b)[1] = (l) >> 8;	\
	    (b)[2] = (l);	\
	} while (0)

#define l2b32(b, l)		\
	do {			\
	    (b)[0] = (l) >> 24;	\
	    (b)[1] = (l) >> 16;	\
	    (b)[2] = (l) >> 8;	\
	    (b)[3] = (l);	\
	} while (0)

#define b2l16(b)  (((uint16) ((b)[0]) << 8) | ((uint16) ((b)[1])))


#define b2l24(b)  (((uint32) ((b)[0]) << 16) | \
		   ((uint32) ((b)[1]) <<  8) | \
		   ((uint32) ((b)[2])))

#define b2l32(b)  (((uint32) ((b)[0]) << 24) | \
		   ((uint32) ((b)[1]) << 16) | \
		   ((uint32) ((b)[2]) <<  8) | \
		   ((uint32) ((b)[3])))


/********************************/
/*	Internal Routines	*/
/********************************/

#ifndef NDEBUG

static void
dump_scsi_cmd(cp)
cmd_t *	cp;
{
    int i;

    DPRINTF(2, ("scsi cmd: sz = %d", cp->ccb.cmdlen));
    for (i = 0; i < cp->ccb.cmdlen; i++)
	DPRINTF(2, (" %02x", cp->ccb.cmd[i]));
    DPRINTF(2, ("\n"));
}


static void
errordump(cp)
cmd_t *	cp;
{
    int i;

    DPRINTF(2, ("aha ccb dump:"));
    for (i = 0; i < sizeof (cp->ccb); i++) {
	if (i % 26 == 0)
	    DPRINTF(2, ("\n"));
	DPRINTF(2, (" %02x", ((byte *) &cp->ccb)[i]));
    }
    DPRINTF(2, ("\n"));
}

#else
#define errordump(x)
#define dump_scsi_cmd(x)
#endif /* NDEBUG */




/*
 * Issue a command to the SCSI card.  Note this is not issuing commands to
 * the SCSI bus but just to chat with the card's local "intelligence".
 */

static void
aha_command(addr, outlen, outptr, inlen, inptr)
vir_bytes	addr;	/* Address of SCSI card's registers */
int		outlen;
byte *		outptr;
int		inlen;
byte *		inptr;
{
    int i;

    /* Send command bytes. */
    for (i = 0; i < outlen; i++) {
	/* Wait for board to be ready. - timeout ?! */
	while (in_byte(AHA_STATREG(addr)) & AHA_CDF)
	    /* do nothing */;
	out_byte(AHA_DATAREG(addr), *outptr++);
    }

    /* Receive data bytes. */
    for (i = 0; i < inlen; i++) {
	/* Wait for the data to be ready. - timeout ?! */
	while (!(in_byte(AHA_STATREG(addr)) & AHA_DF)
		&& !(in_byte(AHA_INTRREG(addr)) & AHA_HACC))
	    /* do nothing */;
	*inptr++ = in_byte(AHA_DATAREG(addr));
    }

    /* Wait for command completed. - timeout ?! */
    while (!(in_byte(AHA_INTRREG(addr)) & AHA_HACC))
	/* do nothing */;
    out_byte(AHA_CNTLREG(addr), AHA_IRST);	/* clear interrupt */

    /* !! should check status register here for invalid command */
}


static errstat
scsi_reset(ctlr)
int32	ctlr;
{
    int		i;
    int		stat;
    int		retries;
    byte	cmd[5];
    byte	haidata[4];
    byte	getcdata[3];
    byte	extbios[2];
    char *	what;
    scsi_ctlr *	c;
    static int	first_time;
    ctlr_t *	ctp;

    c = &Scsi_ctlr_tab[ctlr];
    ctp = &Ctlrs[ctlr];

    /* Reset controller, wait for self test to complete. */
    retries = AHA_TIMEOUT;
    out_byte(AHA_CNTLREG(c->c_addr), AHA_HRST);
    while ((stat = in_byte(AHA_STATREG(c->c_addr))) & AHA_STST
							&& --retries != 0) {
	(void) await((event) 0, (interval) 100);
    }

    if (retries == 0) {
	DPRINTF(0, ("aha0: AHA154x controller not responding\n"));
	return STD_NOTFOUND;
    }

    if (stat == 0) {
	DPRINTF(0, ("aha0: AHA154x controller not present\n"));
	return STD_NOTFOUND;
    }

    /* Check for self-test failure. */
    if ((stat & (AHA_DIAGF | AHA_INIT | AHA_IDLE | AHA_CDF | AHA_DF))
						!= (AHA_INIT | AHA_IDLE)) {
	printf("aha0: AHA154x controller failed self-test\n");
	return STD_SYSERR;
    }

    /* Let the slow devices have a few seconds to recover - especially tapes */
    ctp->lbolt = getmilli() + RESET_TIMEOUT;

    /* !! maybe a sanity check here: make sure IDLE and INIT are set? */

    /* Get information about controller type and configuration. */
    cmd[0] = AHACOM_HAINQUIRY;
    aha_command((vir_bytes) c->c_addr, 1, cmd, 4, haidata);

    cmd[0] = AHACOM_GETCONFIG;
    aha_command((vir_bytes) c->c_addr, 1, cmd, 3, getcdata);

    /* First byte tells what type of board. */
    switch (haidata[0]) {
    case 0x20:
	what = "BusLogic 545";
	haidata[1]= haidata[2]= haidata[3]= '?';	/* forgets these */
	break;
    case 0x31:	what = "Adaptec 1540";		break;
    case 0x41:	what = "Adaptec 154xA/B";	break;
    case 0x42:	what = "Adaptec 1640";		break;
    case 0x43:	what = "Adaptec 1542C";		break;
    case 0x44:
    case 0x45:	what = "Adaptec 1542CF";	break;
    default:	what = "Adaptec ????";
		printf("scsi_reset: unknown Adaptec card type 0x%x\n",
				haidata[0]);
		break;
    }

    if (first_time == 0) {
	printf("%s at addr 0x%x, irq %d\n", what, c->c_addr, c->c_vecinfo);
	first_time = 1;
    }

    /* Disable the 1542C or 1542CF's extended BIOS to unlock the mailbox
     * interface.  (Assume that later boards (> 0x44) share this quirk.)
     */
    if (haidata[0] >= 0x43) {
	cmd[0] = AHACOM_EXTBIOS;
	aha_command((vir_bytes) c->c_addr, 1, cmd, 2, extbios);
	cmd[0] = AHACOM_MBOX_ENABLE;
	cmd[1] = 0;
	cmd[2] = extbios[1];	/* lock code to unlock mailbox */
	aha_command((vir_bytes) c->c_addr, 3, cmd, 0, (byte *) 0);
    }

    /* Set up the DMA channel. */
    switch (getcdata[0]) {
    case 0x80:		/* channel 7 */
	out_byte(0xD6, 0xC3);
	out_byte(0xD4, 0x03);
	break;
    case 0x40:		/* channel 6 */
	out_byte(0xD6, 0xC2);
	out_byte(0xD4, 0x02);
	break;
    case 0x20:		/* channel 5 */
	out_byte(0xD6, 0xC1);
	out_byte(0xD4, 0x01);
	break;
    case 0x01:		/* channel 0 */
	out_byte(0x0B, 0x0C);
	out_byte(0x0A, 0x00);
	break;
    default:
	printf("aha0: AHA154x: strange DMA channel\n");
	return STD_SYSERR;
    }

    /*
     * Initialize mailboxes.  The 1542C wants to have the address of the
     * mailboxes fairly early on.
     */
    for (i = 0; i < NR_MAILBOXES; i++) {
	ctp->mailbox[0][i].status = AHA_MBOXFREE; /* Outgoing mailbox. */
	ctp->mailbox[1][i].status = AHA_MBOXFREE; /* Incoming mailbox. */
	ctp->outptr = ctp->mailbox[0];
    }

    /* Tell controller where the mailboxes are and how many. */
    cmd[0] = AHACOM_INITBOX;
    cmd[1] = NR_MAILBOXES;
    l2b24(cmd + 2, VTOP(ctp->mailbox));
    aha_command((vir_bytes) c->c_addr, 5, cmd, 0, (byte *) 0);

    /* !! maybe sanity check: check status reg for initialization success */

    /* Set bus on, bus off and transfer speed. */
    cmd[0] = AHACOM_BUSON;
    cmd[1] = 7;
    aha_command((vir_bytes) c->c_addr, 2, cmd, 0, (byte *) 0);

    cmd[0] = AHACOM_BUSOFF;
    cmd[1] = 4;
    aha_command((vir_bytes) c->c_addr, 2, cmd, 0, (byte *) 0);

    /* Set SCSI selection timeout. */
    cmd[0] = AHACOM_SETIMEOUT;
    cmd[1] = SCSI_TIMEOUT != 0;		 /* timeouts on/off */
    cmd[2] = 0;				 /* reserved */
    cmd[3] = (SCSI_TIMEOUT >> 8) & 0xFF; /* MSB */
    cmd[4] = SCSI_TIMEOUT & 0xFF;	 /* LSB */
    aha_command((vir_bytes) c->c_addr, 5, cmd, 0, (byte *) 0);

    return STD_OK;
}


static void
scsi_intr(vecinfo)
int	vecinfo;
{
    int32	ctlr;
    scsi_ctlr *	c;
    mailbox_t *	inptr;
    phys_bytes	ccb_phys;
    ctlr_t *	ctp;

    DPRINTF(4, ("scsi_intr(%x)\n", vecinfo ));

    /* Figure out which controller it was */
    for (ctlr = 0, c = Scsi_ctlr_tab; ; c++, ctlr++)
    {
	if (ctlr >= (int32) Num_scsi_ctlrs)
	{
	    printf("scsi_intr: invalid vector (%x)\n", vecinfo);
	    return;
	}
	if (c->c_vecinfo == vecinfo)
	    break; /* Got 'im */
    }

    /*
     * If it was a mailbox-in interrupt then we had better wake up the
     * client.
     */
    if (in_byte(AHA_INTRREG(c->c_addr)) & AHA_MBIF) {
	/* Only incoming mail deserves a message. */
	out_byte(AHA_CNTLREG(c->c_addr), AHA_IRST);	/* clear interrupt */

	ctp = &Ctlrs[ctlr];

	/* Find out which command(s) must get a wakeup */
	for (inptr = ctp->mailbox[1]; inptr < ctp->mailbox[1] + NR_MAILBOXES;
								    inptr++) {
	    if (inptr->status != AHA_MBOXFREE) {
		ccb_phys = b2l24(inptr->ccbptr);
		assert(ccb_phys != 0);
		enqueue((void (*) _ARGS((long))) wakeup, (long) ccb_phys);
	    }
	}
    }
}


static mailbox_t *
get_out_mbox(ctp)
ctlr_t *	ctp;
{
    int i;

    i = 0;
    while (ctp->outptr->status != AHA_MBOXFREE)
    {
	/* find a free mailbox or go to sleep */
	if (++ctp->outptr == ctp->mailbox[0] + NR_MAILBOXES)
	    ctp->outptr = ctp->mailbox[0];
	if (++i == NR_MAILBOXES)
	{
	    await((event) ctp, (interval) 0);
	    i = 0;
	}
    }
    return ctp->outptr;
}



/****************************************/
/*	External Interface Routines	*/
/****************************************/

/*
 * scsi_cmd
 *	Starts up a SCSI command.  With this board the irq_flag is irrelevant.
 *	The Request Sense command also requires special treatment since the
 *	board will have done it for us already.  Therefore when we see it we
 *	simply return STD_OK.
 */

/*ARGSUSED*/
void
scsi_cmd(ctlr, unit, irq_flag)
int32	ctlr;           /* SCSI controller number */
int	unit;           /* target & logical unit to send command to */
int	irq_flag;       /* unused?? */
{
    scsi_unit *	p;
    scsi_ctlr *	c;
    int		target;
    int		i;
    phys_bytes	ccb_phys;
    char *	cp;
    mailbox_t *	inptr;
    ctlr_t *	ctp;
    cmd_t *	com;	/* Where we keep our copy of the command */
    mailbox_t *	outptr;
    size_t	tmp_sz;
    char *	tmp_dataptr;
    interval	delay;

    c = &Scsi_ctlr_tab[ctlr];
    p = c->c_unit[unit];
    target = unit >> 3;

    DPRINTF(0, ("scsi_cmd: (%d, %d, %d)\n", ctlr, unit, irq_flag));
    DPRINTF(0, ("scsi_cmd: dp=0x%x, dsz=0x%x, w=%x, csz=%x\n",
	    p->u_data, p->u_datasz, (p->u_flags & SF_WRITE)?1:0, p->u_cmdsz));
    if (target < 0 || target >= 8 || p == 0 || p->u_datasz < 0 ||
		(p->u_datasz > MAX_DMA_BYTES) ||
		(p->u_datasz != 0 && p->u_data == 0) ||
		(p->u_cmdsz != 6 && p->u_cmdsz != 10 && p->u_cmdsz != 12))
    {
	p->u_errstat = STD_ARGBAD;
	DPRINTF(0, ("scsi_cmd: return 1"));
	return;
    }

    switch (p->u_cmdsz)
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
	panic("scsi_cmd: invalid command size %d\n", p->u_cmdsz);
	/*NOTREACHED*/
    }
    if (*cp == REQUEST_SENSE)
    {
	/*
	 * If there was any sense data last time we report its size with the
	 * following hack.  The sense data is already available and so we
	 * simply indicate to the upper layer how big it is, since we
	 * recorded it last time through.
	 */
	p->u_datasz -= p->u_sensesz;
	p->u_errstat = STD_OK;
	return;
    }
    p->u_sensesz = 0;

    ctp = &Ctlrs[ctlr];

    /* See if it is safe to start a command */
    if ((delay = ctp->lbolt - getmilli()) > 0)
	await((event) 0, delay);
    else
	ctp->lbolt = getmilli(); /* Avoid timer wrap problems */

    /*
     * We can't DMA above 16 MB so we have to DMA & then copy. :-(
     */
    if (p->u_datasz != 0 && (uint32) (p->u_data + p->u_datasz) >= LIM_16MB)
    {
	mu_lock(&ctp->mtx);
	if (p->u_flags & SF_WRITE)
	{
	    memcpy((_VOIDSTAR) ctp->buffer, (_VOIDSTAR) p->u_data,
							(size_t) p->u_datasz);
	}
	else /* save the size till later */
	    tmp_sz = p->u_datasz;
	tmp_dataptr = (char *) p->u_data;
	p->u_data = ctp->buffer;
    }
    else
	tmp_dataptr = (char *) 0;

    /* Start a command */
    /*******************/

    /* Find a free command block */
    com = &Cmdblk[Cmbnext];
    if (++Cmbnext >= NR_CMDBLKS)
	Cmbnext = 0;
    mu_lock(&com->ccb_mtx);

    /* Init ccb */
    com->ccb.opcode = CCB_INIT;
    com->ccb.senselen = AHA_MAXSENSE_SZ;	/* always want sense info */
    l2b24(com->ccb.linkptr, 0L);		/* never link commands */
    com->ccb.linkid = 0;
    com->ccb.reserved[0] = 0;
    com->ccb.reserved[1] = 0;
    com->ccb.addrcntl = ccb_scid(target) | ccb_lun(unit); /* no length checks */
    l2b24(com->ccb.datalen, p->u_datasz);
    l2b24(com->ccb.dataptr, (phys_bytes) p->u_data);
    compare((int) p->u_data, <, LIM_16MB);
    compare((int) (p->u_data + p->u_datasz), <, LIM_16MB);
    com->ccb_phys = VTOP(&com->ccb);
    compare(com->ccb_phys, <, LIM_16MB);
    com->ccb.cmdlen = p->u_cmdsz;
    (void) memmove((_VOIDSTAR) com->ccb.cmd, (_VOIDSTAR) cp,
							(size_t) p->u_cmdsz);

    dump_scsi_cmd(com);

    /* Find a free out-mailbox and use it */
    outptr = get_out_mbox(ctp);
    l2b24(outptr->ccbptr, com->ccb_phys);
    outptr->status = AHA_MBOXSTART;

    /* Tell the controller to execute the SCSI command. */
    out_byte(AHA_DATAREG(c->c_addr), AHACOM_STARTSCSI);

    /* Wait for the SCSI command to complete */
    /*****************************************/

    inptr = ctp->mailbox[1];
    for (i = 0;;) {
	/* Look for an incoming message. */
	if (++inptr == ctp->mailbox[1] + NR_MAILBOXES)
	    inptr = ctp->mailbox[1];
	if (inptr->status != AHA_MBOXFREE) {
	    ccb_phys = b2l24(inptr->ccbptr);
	    if (ccb_phys == com->ccb_phys) /* It's ours */
		break;
	}

	if (++i == NR_MAILBOXES)
	{
	    /* No mail yet, wait for an interrupt. */
	    if (await((event) com->ccb_phys, (interval) p->u_timeout) != 0)
	    {
		/* We need to kill off the command to free the mailbox */
		outptr = get_out_mbox(ctp);
		l2b24(outptr->ccbptr, com->ccb_phys);
		outptr->status = AHA_MBOXABORT;
		printf("scsi_cmd: no mailbox reply (ctlr %d, unit %d)\n",
								ctlr, unit);
		p->u_errstat = STD_SYSERR;
		if (tmp_dataptr != 0)
		    mu_unlock(&ctp->mtx);
		mu_unlock(&com->ccb_mtx);
		return;
	    }
	    i = 0;
	}
    }

    /* Check the results of the command */
    /************************************/

    inptr->status = AHA_MBOXFREE;	/* free up inbox */

    /* Check the results of the operation. */

    if (com->ccb.hastat != 0) {
	memset((_VOIDSTAR) ccb_sense(&com->ccb), 0, AHA_MAXSENSE_SZ);

	/* Did we get a select timeout - probably no device there */
	if (com->ccb.hastat == HST_TIMEOUT)
	    p->u_errstat = SERR_SELFAIL;
	else
	{
	    /* Weird host adapter status. */
	    printf("scsi_cmd: host adaptor err 0x%02x\n", com->ccb.hastat);
	    errordump(com);
	    p->u_errstat = STD_SYSERR;
	}
	if (tmp_dataptr != 0)
	    mu_unlock(&ctp->mtx);
	mu_unlock(&com->ccb_mtx);
	return;
    }

    switch (com->ccb.tarstat)
    {
    case STAT_GOOD:
	p->u_errstat = STD_OK;
	/* If we are reading to above 16MB then we have to copy the data */
	if (tmp_dataptr != 0 && !(p->u_flags & SF_WRITE))
	    memcpy((_VOIDSTAR) tmp_dataptr, (_VOIDSTAR) p->u_data, tmp_sz);
	break;

    case STAT_CHECK_CON:
	/* We got sense data so we copy it to the caller's buffer */
	memmove((_VOIDSTAR) p->u_sense, (_VOIDSTAR) ccb_sense(&com->ccb),
							AHA_MAXSENSE_SZ);
	/* The size of the sense data is embedded in the data */
	if ((*ccb_sense(&com->ccb) & 0x70) != 0x70)
	    p->u_sensesz = 4;
	else /* Extended sense */
	{
	    p->u_sensesz = 8 + (*(ccb_sense(&com->ccb) + 7) & 0xFF);
	    if (p->u_sensesz > AHA_MAXSENSE_SZ)
		p->u_sensesz = AHA_MAXSENSE_SZ;
	}
	DPRINTF(0, ("got %d bytes sense\n", p->u_sensesz));
	p->u_errstat = SERR_CHECKCON;
	break;
    
    case STAT_BUSY:
	p->u_errstat = SERR_AGAIN;
	break;

    default:
	p->u_errstat = STD_SYSERR;
	break;
    }

    if (tmp_dataptr != 0)
	mu_unlock(&ctp->mtx);
    mu_unlock(&com->ccb_mtx);

    /* Free the command slot */
    wakeup((event) ctp);
    return;
}


/*
 * scsi_ctlr_init
 *      Initialise SCSI Controller
 */

/*ARGSUSED*/
errstat
scsi_ctlr_init(devaddr, ivec)
int32	devaddr;
int	ivec;	/* unused */
{
    scsi_ctlr *	c;
    int32	ctlr;
    ctlr_t *	ctp;	/* controller pointer */
    errstat	err;

    DPRINTF(0, ("scsi_ctlr_init: 0x%x %d\n", devaddr, ivec));

    /* See if we already know about this controller */
    for (c = Scsi_ctlr_tab, ctlr = 0; ctlr < Num_scsi_ctlrs; c++, ctlr++)
    {
	if (c->c_addr == devaddr)
	{
	    /* We found our controller */
	    break;
	}
    }

    ctp = &Ctlrs[ctlr];
    if (ctlr >= Num_scsi_ctlrs)
    {
	/* We have a new controller! */
	if (++Num_scsi_ctlrs > MAX_SCSI_CONTROLLERS)
	{
	    DPRINTF(0, ("scsi_ctlr_init: found %d ctlrs - only support %d\n",
			    Num_scsi_ctlrs, MAX_SCSI_CONTROLLERS));
	    return STD_NOTFOUND;
	}
	c->c_addr = devaddr;
	c->c_vecinfo = ivec;
	mu_init(&ctp->mtx);
    }

    mu_lock(&ctp->mtx);

    /* Now we know which controller it is */
    if (!c->c_initted)
    {
	if ((err = scsi_reset(ctlr)) != STD_OK)
	{
	    Num_scsi_ctlrs--;
	    c->c_addr = 0;
	}
	else
	{
	    c->c_initted = 1;
	    setirq(ivec, scsi_intr);
	    pic_enable(c->c_vecinfo);
	    Sd_Lim_blk_cnt = MAX_DMA_BYTES / 512;
	}
    }
    else
    {
	err = STD_OK;
    }
    mu_unlock(&ctp->mtx);

    return err;
}


#include "disk/disk.h"
#include "vdisk/disklabel.h"


errstat
scsi_geom(devaddr, unit, geom)
long		devaddr;
int		unit;
geometry *	geom;
{
    int32	lastblk;
    int32	blksz;
    uint32	numblks;	/* # 0.5K blocks on disk */
    errstat	err;

    DPRINTF(0, ("called scsi_geom(%d, %d, %x)\n", devaddr, unit, geom));

    if ((err = sd_capacity(devaddr, unit, &lastblk, &blksz)) != STD_OK)
	return err;
    numblks = blksz / 512 * lastblk;

    geom->g_bpt = 0;
    geom->g_bps = 0;
    geom->g_altcyl = 0;

    if (numblks > 2048 * 1024)
    {
	/* The disks is > 1GB */
	geom->g_numhead = 255;
	geom->g_numsect = 63;
    }
    else
    {
	/* Little disk :-) */
	geom->g_numhead = 64;
	geom->g_numsect = 32;
    }
    geom->g_numcyl = numblks / (geom->g_numhead * geom->g_numsect);

    return STD_OK;
}
