/*	@(#)sun4c_eth.c	1.2	96/02/27 13:54:04 */
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
#include "map.h"
#include "seg.h"
#include "randseed.h"
#include "debug.h"
#include "sys/proto.h"
#include "sys/flip/eth_if.h"
#include "mmuproto.h"

#define MAX_ETHER_INTERFACES	4
#define ETHERNET_ADDRESS_SIZE	6

#define NSEGS_FOR_ETHER		2
#define MAP_SIZE		NSEGS_FOR_ETHER*MMU_SEGSIZE
#define eth_last		( LANCEMEM + MAP_SIZE )

void idprom_eaddr _ARGS((char *));

/* Forward declarations */
static int	sunethaddr();
static char *	etheralloc();
static uint32	ethermap();

#define MMU_SETPTE(a, b, c) do { \
	DPRINTF(2, ("mmu_setpte(%x,%x,%x)\n", a, b, c)); \
	mmu_setpte(a,b,c); } while(0)

static vir_bytes eth_first;	/* Our first mapping address */
static vir_bytes eth_cur;	/* Current page */


#include "interrupt.h"
#include "glainfo.h"

/*
 * Template for what la_info should look like.  We need this since initialised
 * static data is made read-only.
 */
static lai_t la_infotempl =
    { LANCEMEM, {INT_LANCE},   7, LOGNRECV, LOGNTRANS,    0,    0, 100,
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
    vir_bytes oaddr;
    vir_bytes addr;

    if ( LANCEMEM <= fromaddr && fromaddr < eth_last ) {
	DPRINTF(2, ( "ethermap: direct %x\n", fromaddr ));
	fromaddr &= 0xFFFFFF;
	return( fromaddr );
    }

    addr = eth_cur;
    if ( addr + 2*PAGESIZE >= eth_last ) {
	addr = eth_first;
    }
    oaddr = ( addr & 0xFFFFFF ) | ( fromaddr & ( PAGESIZE - 1 ));

    /* Map the first page */
    DPRINTF(2, ( "ethermap1: %x (%x) gets %x\n",
	   addr, fromaddr, PTE_NOCACHE | MMU_MAKEPTE( fromaddr )));
    MMU_SETPTE( KERN_CTX, addr,
	       PTE_NOCACHE | MMU_MAKEPTE( fromaddr ));
    fromaddr += PAGESIZE;
    addr += PAGESIZE;

    /* Map the second page */
    DPRINTF(2, ( "ethermap2: %x (%x) gets %x\n",
	   addr, fromaddr, PTE_NOCACHE | MMU_MAKEPTE( fromaddr )));
    MMU_SETPTE( KERN_CTX, addr, PTE_NOCACHE | MMU_MAKEPTE( fromaddr ));
    fromaddr += PAGESIZE;
    addr += PAGESIZE;

    eth_cur = addr;
    DPRINTF(2, ( "ethermap returns %x (%x)\n", oaddr, eth_cur ));
    return( oaddr );
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
    vir_bytes memp, lancep, olancep;

    DPRINTF(1, ("etheralloc(%d, %d)=", size, align));
    size = RNDUP( size, PAGESIZE );
    memp = getblk( size );
    if ( memp ) {
	olancep = lancep = eth_first;
	eth_first += size;
	if ( eth_cur < eth_first ) eth_cur = eth_first;

	for ( ; size > 0; size -= PAGESIZE ) {
	    DPRINTF(2, ( "etheralloc: %x gets %x\n",
	      		lancep, PTE_NOCACHE | MMU_MAKEPTE( memp )));
	    MMU_SETPTE( KERN_CTX, lancep, PTE_NOCACHE | MMU_MAKEPTE( memp ));
	    MMU_SETPTE( KERN_CTX, memp, PTE_NOCACHE | MMU_MAKEPTE( memp ));
	    /*DBG*/ * (char *) memp = 0;
	    /*DBG*/ * (char *) lancep = 0;
	    memp += PAGESIZE;
	    lancep += PAGESIZE;
	}

	DPRINTF(1, ( "%x\n", olancep ));
	return( (char *) olancep );
    } else {
	printf( "etheralloc returns NULL\n" );
	return( 0 );
    }
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

    proplen = obp_getattr((dnode_t) param, "mac-address", (void *) buf,
							sizeof (buf));
    if (proplen < ETHERNET_ADDRESS_SIZE) {
	/* Get it from the id prom */
	idprom_eaddr(eaddr);
    }
    else
	memmove((_VOIDSTAR) eaddr, (_VOIDSTAR) buf, ETHERNET_ADDRESS_SIZE);
    for (i = 3; i < 6; i++) {
	randseed((unsigned long) eaddr[i], 8, RANDSEED_HOST);
    }
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
    char	pathname[MAXDEVNAME];
    dev_reg_t	d;
    long	intrdata[2];
    int		proplen;

    if (p == 0)
	return;
    if (p->l_prevp != 0)
    {
	(void) obp_getattr(p->l_prevp->l_curnode, "name",
					    (void *) pathname, MAXDEVNAME);
	if (strcmp(pathname, "lebuffer") == 0 || ndev >= MAX_ETHER_INTERFACES)
	    return;
    }
    proplen = obp_getattr(p->l_curnode, "intr", (void *) intrdata,
							    sizeof intrdata);

    if (obp_genname(p, pathname, pathname + MAXDEVNAME) != 0)
	obp_devinit(pathname);
    obp_devaddr(p, &d);
    MMU_SETPTE( KERN_CTX, eth_first, (int) ((( d.reg_addr ) >> PAGESHIFT ) |
	   PTE_KERNWRITE | PTE_SPACE(d.reg_bustype) | PTE_NOCACHE ));
    
    la_info[ndev] = la_infotempl;
    /* Fill in the interrupt vector if it is specified in the device */
    if (proplen == sizeof intrdata)
	la_info[ndev].lai_intrinfo.ii_vector = INTERRUPT(intrdata[0]);
    la_info[ndev].lai_getarg = p->l_curnode;
    la_info[ndev].lai_devaddr = eth_first;
    eth_first += PAGESIZE;
    ndev++;
    /* Increase the count of lance devices found */
    heilist[0].hei_nif = ndev;
}


/*
 * Map the lance chip in at virtual address LANCEMEM - the base of the
 * lance's virtual address world.  The physical address of the lance is
 * DVADR_LANCE (see map.h).  This routine is called by initmm().
 */
void
etherinit() {


    if ( eth_first == 0 ) eth_first = eth_cur = LANCEMEM;

    /*
     * The ethernet interface list needs to have the number of interfaces
     * filled in in the template and this is done by build_ether.
     * The rest of the initialisation we do here.  Note that since heilist
     * is in bss, heilist[1] is already null.
     */
    heilist[0] = heitemplate;
    mmu_populate( (vir_bytes) eth_first, MAP_SIZE );

    obp_regnode("name", "le", build_ether);
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
