/*	@(#)sun4m_eth.c	1.2	96/02/27 13:58:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "openprom.h"
#include "mmuconst.h"
#include "mmuproto.h"
#include "map.h"
#include "seg.h"
#include "randseed.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "sys/proto.h"
#include "sys/flip/eth_if.h"

#define MAX_ETHER_INTERFACES	4
#define ETHERNET_ADDRESS_SIZE	6

static char *lancemem;
static char *cur_lancemem;
static vir_bytes lancememsize;
uint32 io_lancemem;

void idprom_eaddr _ARGS((char *));

/* Forward declarations */
static int	sunethaddr();
static char *	etheralloc();
static uint32	ethermap();

#include "interrupt.h"
#include "glainfo.h"

/*
 * Template for what la_info should look like.  We need this since initialised
 * static data is made read-only.
 */
static lai_t la_infotempl =
    { 0, {INTERRUPT(6)},   7, LOGNRECV, LOGNTRANS,    0,    0, 100,
      sunethaddr,   0,         etheralloc,         ethermap };

lai_t la_info[MAX_ETHER_INTERFACES];

#include "ethif.h"

int la_alloc();
int la_init(); void la_send();
int la_setmc(), la_stop();

static hei_t heitemplate =
	{ "lance", 0, la_alloc, la_init, la_send, la_setmc, la_stop };

hei_t heilist[2];


/*
 * ethermap() (DEVICE ENTRY POINT) -- map the given address into an address
 * that the lance chip can reach
 */
/* ARGSUSED */
static uint32
ethermap( fromaddr, size )
phys_bytes fromaddr;
uint32 size;
{
	uint32 ioaddr;
	uint32 lancemem_offset;

	lancemem_offset = (uint32) fromaddr - (uint32) lancemem;
	if (lancemem_offset < lancememsize) {
		compare(lancemem_offset + size, <=, lancememsize);
		ioaddr = io_lancemem + lancemem_offset;
	} else {
		ioaddr = iommu_circbuf(fromaddr, size);
	}
	compare(ioaddr&0xFF000000, ==, 0xFF000000);
	return ioaddr&0xFFFFFF;
}


/*
 * etheralloc() (DEVICE ENTRY POINT) -- allocate data structures from kernel
 * memory and map them into the lance's IO space, returning the lance address.
 */
/*ARGSUSED*/
static char *
etheralloc( size, align )
vir_bytes size;
int align;
{
	char *result;

	if (align) {
		while (((long) cur_lancemem)&(align-1))
			cur_lancemem++;
	}
	result = cur_lancemem;
	cur_lancemem += size;
	DPRINTF(1, ("etheralloc(%d, %d) = %x, leaving %d\n",
		size, align, result, lancememsize - (cur_lancemem - lancemem)));
	if (cur_lancemem - lancemem > lancememsize)
		panic("underestimated lance memory use");
	return result;
}


/*ARGSUSED*/
static int
sunethaddr(param, eaddr)
long param;
char *eaddr;
{
    int proplen;
    register i;
    char buf[10];

    /*
     * On the Classic the mac-address attribute is 7 bytes long ???
     * Some day we will see the wisdom of this, but for now
     * we hack around it.
     */

    DPRINTF(2, ("sunethaddr called\n"));
    proplen = obp_getattr((dnode_t) param, "mac-address", (void *) buf,
							sizeof(buf));
    if (proplen >= ETHERNET_ADDRESS_SIZE) {
	memcpy((_VOIDSTAR) eaddr, (_VOIDSTAR) buf, ETHERNET_ADDRESS_SIZE);
	for (i = 0; i < 6; i++) {
	    randseed((unsigned long) eaddr[i], 8, RANDSEED_HOST);
	}
    }
    else
	idprom_eaddr(eaddr);	/* Get eaddr from idprom */
    return 1;
}


/*
 * build_ether
 *   Map in the ethernet device and record its nodeid in the la_info
 *   struct so that when sunethaddr() is called it knows which ethernet
 *   address to lookup.
 */
static void
build_ether(p)
nodelist *	p;
{
    static int	ndev;
    dev_reg_t	d;
    char	pathname[MAXDEVNAME];
#ifdef notdef
    int		proplen;
    long	intrdata[2];
#endif

    if (p == 0)
	return;
    compare (ndev, <, MAX_ETHER_INTERFACES);

    /*
     * If we boot from disk the ethernet device is not initialised.  We do
     * a prom open of the device so that it will provide the mac-address
     * attribute later on.  Since some sun4's have both AUI and twisted pair
     * we have to send a packet so that the prom figures out which one is
     * actually connected.  We don't want to do it ourselves.
     */
    assert(prom_devp->op_romvec_version >= 2);
    if (obp_genname(p, pathname, pathname + MAXDEVNAME) != 0) {

	static char buf[64]; /* A "safe" (TM) bogus ethernet packet */
	ihandle_t fd;
	int n;

	if ((fd = (*prom_devp->op_open)(pathname)) != 0) {
	    if ((n = (*prom_devp->op_write)(fd, buf, 64)) != 64) {
		printf("build_ether: WARNING: op_write returned %d\n", n);
	    }
	} else {
	    printf("\nbuild_ether: WARNING: open of %s failed\n", pathname);
	    printf("*** This Ethernet card is not supported!! ***\n\n");
	    return;
	}
    }

    la_info[ndev] = la_infotempl;
    la_info[ndev].lai_getarg = p->l_curnode;
    obp_devaddr(p, &d);
    la_info[ndev].lai_devaddr = mmu_mapdev(&d);
#ifdef notdef
    /* Fill in the interrupt vector if it is specified in the device */
    proplen = obp_getattr(p->l_curnode, "intr", (void *) intrdata,
							    sizeof intrdata);
    if (proplen == sizeof intrdata) {
	DPRINTF(1, ("le interrupts at vector %d\n", INTERRUPT(intrdata[0])));
	la_info[ndev].lai_intrinfo.ii_vector = INTERRUPT(intrdata[0]);
    }
#endif
    /* Guestimate for memory usage */
#ifndef EXTRA_RECEIVE_BUFFERS
    lancememsize += 24 + (1<<la_info[ndev].lai_logrcv)*1580 +
			 (1<<la_info[ndev].lai_logtmt) * 8;
#else
    /* We now allocate twice as much receive buffers as fit in the
     * receive ring.
     */
    lancememsize += 24 + 2 * (1<<la_info[ndev].lai_logrcv)*1580 +
			 (1<<la_info[ndev].lai_logtmt) * 8;
#endif
    /* Increase the count of lance devices found */
    ndev++;
    heilist[0].hei_nif = ndev;
}


/*
 * Map the lance chip in at virtual address LANCEMEM - the base of the
 * lance's virtual address world.  The physical address of the lance is
 * DVADR_LANCE (see map.h).  This routine is called by initmm().
 */
void
etherinit() {

    /*
     * The ethernet interface list needs to have the number of interfaces
     * filled in in the template and this is done by build_ether.
     * The rest of the initialisation we do here.  Note that since heilist
     * is in bss, heilist[1] is already null.
     */
    heilist[0] = heitemplate;

    obp_regnode("name", "le", build_ether);

    cur_lancemem = lancemem = aalloc(lancememsize, 0);
    io_lancemem = iommu_allocbuf((phys_bytes) lancemem, lancememsize);
    mmu_dontcache((vir_bytes) lancemem, lancememsize);
}


#ifndef NO_KERNEL_SECURITY
#include "kparam.h"

fillkparam(kp)
struct kparam *kp;
{
	if (getkparams_from_prom(kp) == 0) {
		/* Nothing in the prom so use ethernet address */
		DPRINTF(1, ("Using insecure kernel capability\n"));
		eth_info(0, (char *) &kp->kp_port, (int *) 0, (int *) 0);
		eth_info(0, (char *) &kp->kp_chkf, (int *) 0, (int *) 0);
	}
}
#else
/*
 * Temporary hack
 */
kernel_port(p)
char *p;
{
	eth_info(0, p, (int *) 0, (int *) 0);
}

#endif
