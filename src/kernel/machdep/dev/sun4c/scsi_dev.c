/*	@(#)scsi_dev.c	1.6	96/02/27 13:53:45 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Routines to probe the openprom to find the SCSI controllers present.
 * This code is called early so that it is ready before the disk/tape
 * servers are started.
 *
 * Author:
 *      Gregory J. Sharp, Aug 1993
 */

#include "stdlib.h"
#include "amoeba.h"
#include "machdep.h"
#include "scsi.h"
#include "scsi_sun4.h"
#include "openprom.h"
#include "interrupt.h"
#include "sys/proto.h"
#include "mmuproto.h"
#include "promid_sun4.h"
#include "memaccess.h"

#include "debug.h"
#include "assert.h"
INIT_ASSERT

extern struct mach_info machine;

scsi_ctlr	Scsi_ctlr_tab[SUN_NUM_SCSI_CONTROLLERS];
scsi_sun4info	Scsi_info[SUN_NUM_SCSI_CONTROLLERS];
unsigned int	Num_scsi_ctlrs; /* Total # controllers found */

/*
 * Some local things needed to track down DMA controllers with early
 * versions of the openprom.
 */
static int		Need_DMA_address;
static dnode_t		Scsi_promnode[SUN_NUM_SCSI_CONTROLLERS];

#define	SEL_TIMER		250 /* ms */

/*
 * scsi_get_dma_ctlr
 *
 *	Take the nodelist pointing to the SCSI device found and hunt for the
 *	matching DMA device.  It must either be the parent of the SCSI device
 *	or one of its siblings.  This routine returns the virtual address of
 *	the DMA chip or 0 if it couldn't find it.
 */

static vir_bytes
scsi_get_dma_ctlr(p)
nodelist *	p; /* Nodelist of the SCSI controller */
{
    vir_bytes	vaddr;
    char	pathname[MAXDEVNAME];
    dev_reg_t	dev;

    assert(p->l_prevp != 0);   /* We can't handle a device tree like this!! */

    /*
     * See if the parent node is a DMA device.
     */
    pathname[0] = '\0';
    (void) obp_getattr(p->l_prevp->l_curnode, "name", (void *) pathname,
							    MAXDEVNAME);
    if (strcmp(pathname, "dma") == 0)
    {
	if (obp_getattr(p->l_prevp->l_curnode, "address", (void *) &vaddr,
					sizeof (vaddr)) == sizeof (vaddr))
	{
	    return vaddr;
	}
	else
	{
	    /* We find the physical address and map it in ourselves */
	    obp_devaddr(p->l_prevp, &dev);
	    return mmu_mapdev(&dev);
	}   
    }
    /*
     * Hunt for a sibling on the same sbus slot.  Alas we can't do that
     * now or the regnode will lose its place in its tree search.  We
     * return 0 for now and find it later.  We set a flag for it.
     */
    Need_DMA_address++;
    return 0;
}


/*
 * This routine is called when the openprom finds a SCSI device (see the call to
 * obp_regnode in scsi_getdevs.
 */

static void
scsi_regdevs(p)
nodelist *	p;
{
    static int	nscsidevs;	/* # scsi ctlrs found */

    vir_bytes	vaddr;
    dev_reg_t	dev;
    uint_t	intr[2];	/* SCSI chip interrupt vector */
    uint_t	clock;		/* SCSI chip clock frequency */
    uint_t	init_id;	/* SCSI initiator id */

    if (nscsidevs >= SUN_NUM_SCSI_CONTROLLERS)
    {
	nscsidevs++;
	DPRINTF(0, ("scsi_getdevs: WARNING found %d SCSI ctlrs\n", nscsidevs));
	DPRINTF(0,
	  ("Only the first %d will be supported\n", SUN_NUM_SCSI_CONTROLLERS));
	return;
    }

    /* Remember the prom node_id */
    Scsi_promnode[nscsidevs] = p->l_curnode;
    
    if (obp_getattr(p->l_curnode, "address", (void *) &vaddr, sizeof (vaddr))
							    == sizeof (vaddr))
    {
	/* The prom has mapped it in for us */
	Scsi_info[nscsidevs].s_scsi_ctlr = vaddr;
    }
    else
    {
	/* We find the physical address and map it in ourselves */
	obp_devaddr(p, &dev);
	Scsi_info[nscsidevs].s_scsi_ctlr = mmu_mapdev(&dev);
    }

    assert(Scsi_info[nscsidevs].s_scsi_ctlr != 0);

    /*
     * Find the corresponding DMA controller for the SCSI controller.
     * It may not be possible to find it just now.  In that case our caller
     * will * have to fill it in later.
     */
    Scsi_info[nscsidevs].s_dma_ctlr = scsi_get_dma_ctlr(p);

    intr[0] = intr[1] = 0;
    if (obp_getattr(p->l_curnode, "intr", (void *) intr, sizeof intr)
								!= sizeof intr)
    {
	DPRINTF(0,
	     ("scsi_getdevs: No INTR field for SCSI node %x\n", p->l_curnode));
	/* We fill in what we can be reasonably sure will work */
	intr[0] = INT_SCSI;
    }
    else
    {
	intr[0] = INTERRUPT(intr[0]);
    }

    Scsi_info[nscsidevs].s_ivec = intr[0];

    /* Figure out the clock conversion factor */
    if (obp_getattr(p->l_curnode, "clock-frequency",
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
    Scsi_info[nscsidevs].s_clockfreq = clock;
    Scsi_info[nscsidevs].s_clk_cnv = (clock + 4) / 5;

    /*
     * The select timeout register set to SEL_TIMER ms
     * In practice it will be a touch shorter since we truncate in our
     * division rather than round, but that is ok.  It is close enough.
     */
    Scsi_info[nscsidevs].s_sel_timeout = 
	clock * SEL_TIMER * 1000 / (8192 * Scsi_info[nscsidevs].s_clk_cnv);

    /*
     * The SCSI initiator id.  IF the PROM knows it, it is in the parent or the
     * grandparent - not in the SCSI node as you'd expect!
     */
    p = p->l_prevp;
    if (p != 0 && obp_getattr(p->l_curnode, "scsi-initiator-id",
			(void *) &init_id, sizeof (uint_t)) == sizeof (uint_t))
    {
	/* The parent knew the initiator-id */
	DPRINTF(0, ("Parent has initiator id %d\n", init_id));
	Scsi_info[nscsidevs].s_initiator_id = init_id;
    }
    else
    {
	p = p->l_prevp;
	if (p != 0 && obp_getattr(p->l_curnode, "scsi-initiator-id",
			(void *) &init_id, sizeof (uint_t)) == sizeof (uint_t))
	{
	    /* The grand-parent knew the initiator-id */
	    DPRINTF(0, ("Grandparent has initiator id %d\n", init_id));
	    Scsi_info[nscsidevs].s_initiator_id = init_id;
	}
	else
	{
	    /* Can't find initiator-id attribute - give a reasonable default */
	    DPRINTF(0, ("Using default initiator id 7\n"));
	    Scsi_info[nscsidevs].s_initiator_id = 7;
	}
    }

    DPRINTF(1, ("found scsi device s=%x d=%x i=%x c=%x t=%x id=%x\n",
		Scsi_info[nscsidevs].s_scsi_ctlr,
		Scsi_info[nscsidevs].s_dma_ctlr,
		Scsi_info[nscsidevs].s_ivec,
		Scsi_info[nscsidevs].s_clk_cnv,
		Scsi_info[nscsidevs].s_sel_timeout,
		Scsi_info[nscsidevs].s_initiator_id));
    nscsidevs++;
    Num_scsi_ctlrs = nscsidevs; /* Update our count of SCSI devices found */
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
    int		i;
    int		ndmadevs;
    device_info	dnodes[SUN_NUM_SCSI_CONTROLLERS];
    dev_reg_t	dreg[SUN_NUM_SCSI_CONTROLLERS];
    dev_reg_t	sreg;

    obp_regnode("name", "esp", scsi_regdevs);

    if (Need_DMA_address)
    {
	/* Get the device info about DMA controllers */
	ndmadevs = obp_findnode("/sbus/dma", dnodes, SUN_NUM_SCSI_CONTROLLERS);
	if (ndmadevs <= 0)
	{
	    panic("scsi_getdevs: No DMA ctlrs found on this host");
	}

	/* For each SCSI ctlr there must be a DMA ctlr. */
	if (ndmadevs < Num_scsi_ctlrs)
	{
	    printf("scsi_getdevs: found %d SCSI ctlrs but only %d DMA ctlrs\n",
						    Num_scsi_ctlrs, ndmadevs);
	    panic("scsi_getdevs: inconsistent openprom");
	}

	/*
	 * Make a copy of all the original dev_reg fields of the DMA ctlrs
	 * so we can compare them with each SCSI ctlr to find the DMA chip on
	 * the same SBUS slot to match with the SCSI ctlr.
	 */
	for (i = 0; i < ndmadevs; i++)
	{
	    if (obp_getattr(dnodes[i].di_nodeid, "reg", (void *) &dreg[i],
				    sizeof (dev_reg_t)) != sizeof (dev_reg_t))
	    {
		DPRINTF(0,
	   ("scsi_getdevs: Can't get device reg info for DMA ctlr at node %x\n",
			    dnodes[i].di_nodeid));
		dreg[i].reg_bustype = -1;
	    }
	}

	/*
	 * Hunt down the missing DMA controllers for the SCSI devices...
	 */
	for (i = 0; i < Num_scsi_ctlrs; i++)
	{
	    if (Scsi_info[i].s_dma_ctlr == 0)
	    {
		DPRINTF(0,
		("scsi_getdevs: looking for DMA ctlr for SCSI device %d\n", i));
		if (obp_getattr(Scsi_promnode[i], "reg", (void *) &sreg,
				    sizeof (dev_reg_t)) == sizeof (dev_reg_t))
		{
		    /* Match DMA controller virtual address */
		    int	j;

		    for (j = 0; j < ndmadevs; j++)
		    {
			if (sreg.reg_bustype == dreg[j].reg_bustype)
			{
			    /* Found it! */
			    if (dnodes[j].di_vaddr != 0)
				Scsi_info[i].s_dma_ctlr = dnodes[j].di_vaddr;
			    else
				Scsi_info[i].s_dma_ctlr =
						mmu_mapdev(&dnodes[j].di_paddr);

			    break;
			}
		    }
		    if (j >= ndmadevs)
		    {
			panic("scsi_getdevs: Can't find DMA device");
		    }
		}
		else
		    panic("scsi_getdevs: can't find reg field for SCSI device");
	    }
	}
    } /* end Need_DMA_address */
}
