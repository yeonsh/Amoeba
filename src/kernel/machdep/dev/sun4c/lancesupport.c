/*	@(#)lancesupport.c	1.2	94/04/06 09:30:27 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: lancesupport.c -- support functions for the generic lance driver
 *
 * Author: Philip Shafer <phil@procyon.ics.Hawaii.Edu>
 *	   (Univerity of Hawai'i, Manoa) November 1990
 */

#include "amoeba.h"
#include "debug.h"
#include "global.h"
#include "machdep.h"
#include "openprom.h"
#include "map.h"
#include "internet.h"
#include "mmuconst.h"
#include "seg.h"
#include "map.h"
#include "mmuproto.h"
#include "sys/proto.h"

#define ETHERNET_ADDRESS_SIZE	6
#define PKT_SIZE		2000
#define NUM_PAGES		2
#define MAX_PACKETS		((1<<LOGNTRANS) + (1<<LOGNRECV))
#define	MAP_SIZE		( NUM_PAGES * MAX_PACKETS * PKT_SIZE )

static vir_bytes eth_first;	/* Our first mapping address */
static vir_bytes eth_cur;	/* Current page */

/* Our last mapping address */
#define eth_last	( LANCEMEM + MAP_SIZE + ( MAP_SIZE >> 1 ))


/*
 * etheraddr() (ENTRY POINT) -- fetch the ethernet address of this machine's
 * ethernet board from the PROM and stuff it into the given buffer.
 */
etheraddr( eaddr )
char *eaddr;
{
    (*prom_devp->pd_eaddr)( ETHERNET_ADDRESS_SIZE, eaddr );
}

/*
 * ethermap() (DEVICE ENTRY POINT) -- map the given address into an address
 * that the lance chip can reach
 */
ethermap( fromaddr )
phys_bytes fromaddr;
{
    vir_bytes oaddr;
    vir_bytes addr;
    int psize = machine.mi_pagesize;

    if ( LANCEMEM <= fromaddr && fromaddr < eth_last ) {
	DPRINTF(2, ( "ethermap: direct %x\n", fromaddr ));
	fromaddr &= 0xFFFFFF;
	return( fromaddr );
    }

    addr = eth_cur;
    if ( addr + psize >= eth_last ) addr = eth_first;
    oaddr = ( addr & 0xFFFFFF ) | ( fromaddr & ( psize - 1 )) ;

    /* Map the first page */
    DPRINTF(2, ( "ethermap1: %x (%x) gets %x\n",
	   addr, fromaddr, PTE_NOCACHE | MMU_MAKEPTE( fromaddr )));
    mmu_setpte( KERN_CTX, addr,
	       PTE_NOCACHE | MMU_MAKEPTE( fromaddr ));
    fromaddr += psize;
    addr += psize;

    /* Map the second page */
    DPRINTF(2, ( "ethermap2: %x (%x) gets %x\n",
	   addr, fromaddr, PTE_NOCACHE | MMU_MAKEPTE( fromaddr )));
    mmu_setpte( KERN_CTX, addr, PTE_NOCACHE | MMU_MAKEPTE( fromaddr ));
    fromaddr += psize;
    addr += psize;

    eth_cur = addr;
    DPRINTF(2, ( "ethermap returns %x (%x)\n", oaddr, eth_cur ));
    return( oaddr );
}

/*
 * etheralloc() (DEVICE ENTRY POINT) -- allocate data structures from kernel
 * memory and map them into the lance's IO space, returning the lance address.
 */
/*ARGSUSED*/
char *etheralloc( size, align )
vir_bytes size;
int align;
{
    vir_bytes memp, lancep, olancep;
    int psize = machine.mi_pagesize;

    size = RNDUP( size, machine.mi_pagesize );
    memp = getblk( size );
    if ( memp ) {
	olancep = lancep = eth_first;
	eth_first += size;
	if ( eth_cur < eth_first ) eth_cur = eth_first;

	for ( ; size > 0; size -= psize ) {
	    DPRINTF(2, ( "etheralloc: %x gets %x\n",
	      		lancep, PTE_NOCACHE | MMU_MAKEPTE( memp )));
	    mmu_setpte( KERN_CTX, lancep, PTE_NOCACHE | MMU_MAKEPTE( memp ));
	    mmu_setpte( KERN_CTX, memp, PTE_NOCACHE | MMU_MAKEPTE( memp ));
	    memp += psize;
	    lancep += psize;
	}

	DPRINTF(2, ( "etheralloc returns %x\n", olancep ));
	return( (char *) olancep );
    } else {
	printf( "etheralloc returns NULL\n" );
	return( 0 );
    }
}

/*
 * Map the lance chip in at virtual address LANCEMEM - the base of the
 * lance's virtual address world.  The physical address of the lance is
 * DVADR_LANCE (see map.h).  This routine is called by initmm().
 */
etherinit() {
    if ( eth_first == 0 ) eth_first = eth_cur = LANCEMEM;

    mmu_populate( (vir_bytes) eth_first, MAP_SIZE );
    mmu_setpte( KERN_CTX, eth_first,
	       (int) ((( (unsigned) DVADR_LANCE ) >> machine.mi_pageshift )
	       | PTE_KERNWRITE | PTE_IO | PTE_NOCACHE ));
    eth_first += machine.mi_pagesize;
}
