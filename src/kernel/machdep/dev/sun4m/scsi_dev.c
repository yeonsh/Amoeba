/*	@(#)scsi_dev.c	1.4	96/02/27 13:58:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Routines to probe the openprom to find the SCSI controllers present.
 * This code is called early so that it is ready before the disk/tape
 * servers are started.
 *
 * Author:
 *	Gregory J. Sharp, Aug, 1993
 */

#include "stdlib.h"
#include "amoeba.h"
#include "machdep.h"
#include "scsi.h"
#include "scsi_sun4.h"
#include "openprom.h"
#include "sys/proto.h"
#include "mmuproto.h"

#include "debug.h"
#include "assert.h"
INIT_ASSERT

/* Variables used by SCSI driver */
scsi_ctlr	Scsi_ctlr_tab[SUN_NUM_SCSI_CONTROLLERS];
scsi_sun4info	Scsi_info[SUN_NUM_SCSI_CONTROLLERS];
unsigned int	Num_scsi_ctlrs; /* Total # actual controllers found */

#define	SEL_TIMER	250 /* ms */


/*
 * scsi_regdev
 *	Routine called by obp_regnode when it finds a device with name esp.
 *	It fills in the table entry for the device.
 */

static void
scsi_regdev(np)
nodelist *	np;
{
    vir_bytes	vaddr;
    dev_reg_t	dev;
    uint_t	intr[2];	/* SCSI chip interrupt vector */
    uint_t	clock;		/* SCSI chip clock frequency */
    uint_t	init_id;	/* SCSI initiator id */
    char	pathname[MAXDEVNAME];
    unsigned	num_scsi_ctlrs;

    DPRINTF(0, ("scsi_regdev: found a SCSI device\n"));
    num_scsi_ctlrs = Num_scsi_ctlrs;
    if (num_scsi_ctlrs >= SUN_NUM_SCSI_CONTROLLERS)
    {
	printf("scsi_regdev: found more than %d SCSI devices. Ignoring them.\n",
			SUN_NUM_SCSI_CONTROLLERS);
	return;
    }

    /* Get the address of the SCSI chip's registers */
    if (obp_getattr(np->l_curnode, "address", (void *) &vaddr, sizeof (vaddr))
							    == sizeof (vaddr))
    {
	/* The prom has mapped it in for us */
	Scsi_info[num_scsi_ctlrs].s_scsi_ctlr = vaddr;
    }
    else
    {
	/* We find the physical address and map it in ourselves */
	obp_devaddr(np, &dev);
	Scsi_info[num_scsi_ctlrs].s_scsi_ctlr = mmu_mapdev(&dev);
    }
    DPRINTF(0, ("scsi_regdev: mapped SCSI device %d in at 0x%x\n",
			num_scsi_ctlrs, Scsi_info[num_scsi_ctlrs].s_scsi_ctlr));
    assert(Scsi_info[num_scsi_ctlrs].s_scsi_ctlr != 0);

    /* Now get the corresponding DMA controller's address */
    assert(np->l_prevp != 0);
    pathname[0] = '\0';
    (void) obp_getattr(np->l_prevp->l_curnode, "name", (void *) pathname,
								    MAXDEVNAME);

    if (strcmp(pathname, "espdma") != 0)
    {
	DPRINTF(0, ("scsi_regdev: found '%s' instead of DMA ctlr\n",
				pathname));
	return;
    }

    if (obp_getattr(np->l_prevp->l_curnode, "address", (void *) &vaddr,
					    sizeof (vaddr)) == sizeof (vaddr))
    {
	Scsi_info[num_scsi_ctlrs].s_dma_ctlr = vaddr;
    }
    else
    {
	/* We find the physical address and map it in ourselves */
	obp_devaddr(np->l_prevp, &dev);
	Scsi_info[num_scsi_ctlrs].s_dma_ctlr = mmu_mapdev(&dev);
    }

    /*
     * Now pick out all the useful info about interrupt vector,
     * initiator-id, etc.
     */
    intr[0] = intr[1] = 0;
    if (obp_getattr(np->l_curnode, "intr", (void *) intr, sizeof intr)
								!= sizeof intr)
    {
	DPRINTF(0,
	     ("scsi_regdev: No INTR field for SCSI node %x\n", np->l_curnode));
	/* We fill in what we can be reasonably sure will work */
	intr[0] = 0x14;
    }
    else
    {
	intr[0] = INTERRUPT(intr[0] & 0xF);
    }
    Scsi_info[num_scsi_ctlrs].s_ivec = intr[0];

    /* Figure out the clock conversion factor */
    if (obp_getattr(np->l_curnode, "clock-frequency",
			(void *) &clock, sizeof (uint_t)) != sizeof (uint_t))
    {
	/* Our best guess at the clock conversion factor */
	DPRINTF(0, ("Taking default for clock freq\n"));
	clock = 25;
    }
    else
    {
	/* Chop off the mega part */
	clock /= 1000000;
    }
    Scsi_info[num_scsi_ctlrs].s_clockfreq = clock;
    Scsi_info[num_scsi_ctlrs].s_clk_cnv = (clock + 4) / 5;

    /*
     * The select timeout register set to SEL_TIMER ms
     * In practice it will be a touch shorter since we truncate in our
     * division rather than round, but that is ok.  It is close enough.
     */
    Scsi_info[num_scsi_ctlrs].s_sel_timeout = 
	clock * SEL_TIMER * 1000 / (8192 * Scsi_info[num_scsi_ctlrs].s_clk_cnv);

    /*
     * The SCSI initiator id.  IF the PROM knows it, it is in the parent or the
     * grandparent - not in the SCSI node as you'd expect!
     */
    np = np->l_prevp;
    if (np != 0 && obp_getattr(np->l_curnode, "scsi-initiator-id",
			(void *) &init_id, sizeof (uint_t)) == sizeof (uint_t))
    {
	/* The parent knew the initiator-id */
	DPRINTF(0, ("Parent has initiator id %d\n", init_id));
	Scsi_info[num_scsi_ctlrs].s_initiator_id = init_id;
    }
    else
    {
	np = np->l_prevp;
	if (np != 0 && obp_getattr(np->l_curnode, "scsi-initiator-id",
			(void *) &init_id, sizeof (uint_t)) == sizeof (uint_t))
	{
	    /* The grand-parent knew the initiator-id */
	    DPRINTF(0, ("Grandparent has initiator id %d\n", init_id));
	    Scsi_info[num_scsi_ctlrs].s_initiator_id = init_id;
	}
	else
	{
	    /* Can't find initiator-id attribute - give a reasonable default */
	    DPRINTF(0, ("Using default initiator id 7\n"));
	    Scsi_info[num_scsi_ctlrs].s_initiator_id = 7;
	}
    }

    DPRINTF(0, ("scsi_regdev: found device s=%x d=%x i=%x c=%x t=%x id=%x\n",
		Scsi_info[num_scsi_ctlrs].s_scsi_ctlr,
		Scsi_info[num_scsi_ctlrs].s_dma_ctlr,
		Scsi_info[num_scsi_ctlrs].s_ivec,
		Scsi_info[num_scsi_ctlrs].s_clk_cnv,
		Scsi_info[num_scsi_ctlrs].s_sel_timeout,
		Scsi_info[num_scsi_ctlrs].s_initiator_id));
    num_scsi_ctlrs++;
    Num_scsi_ctlrs = num_scsi_ctlrs;
}

/*************************************************************************/
/*		External Interface Routines				 */
/*************************************************************************/


/*
 * scsi_init
 *	This routine is called at boot time to inventarize available SCSI
 *	devices.  It first counts the ESP SCSI controllers present and then
 *	mallocs the data structures required and then fills in the structures
 *	with scsi_register_devs.
 */

void
scsi_init()
{
    obp_regnode("name", "esp", scsi_regdev);
}
