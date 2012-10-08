/*	@(#)etherconf.c	1.7	94/04/05 16:41:30 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * etherconf.c
 *
 * Configuration file for AMOEBA's ethernet devices. Currently it contains
 * support dp8390 and lance based boards. The settings more or less reflect
 * our local hardware configurations, but I've tried not to move too far
 * from the vendor's default settings.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <amoeba.h>
#include <type.h>
#include <machdep.h>
#include <internet.h>
#include <map.h>
#include <global.h>
#include <byteorder.h>
#include <ethif.h>
#include <interrupt.h>
#include <sys/flip/packet.h>
#include <sys/flip/ethpreamble.h>
#include <machdep/dev/glainfo.h>
#include <gdp8390.h>
#include <gdpinfo.h>
#include <sys/proto.h>

#ifndef NOLANCE
/*
 * AM2xxx ethernet board configurations:
 *
 * A system with only one adapter with the default factory settings
 * needs the following entry:
 *  { 0x310, 3, 5, 0, 6, 6, 6, 0, 100, am2_ethaddr, 0x300, aalloc, am2_addrconv}
 *
 * It is not possible to have two bus master boards in one machine (actually
 * it is, but it gives too many problems).
 *
 **** Watch out that the address where the lance driver is told to look for a
 **** card does not contain a 3com board.  The lance driver will hang!
 */
int am2_ethaddr();
uint32 am2_addrconv();

#define NLANCE (sizeof(la_info) / sizeof(lai_t))
lai_t la_info[] = {
    { 0x310, { 3, 5 }, 0, 6, 6, 6, 0, 100,
	am2_ethaddr, 0x300, aalloc, am2_addrconv},
};

void la_send();
int la_alloc(), la_init(), la_setmc(), la_stop();
#endif /* NOLANCE */

#ifndef NODP8390
#ifndef NOWD
/*
 * Western Digital ethernet board configurations:
 *
 * A system that has one WD adapter with the default factory settings,
 * needs the following entry:
 *  { 0x280, 3, -1, 0xD0000,
 *	wd_init, wd_gethead, wd_getpkt, wd_putpkt, wd_stop },
 *
 * A system that has two WD adapters and functions as a gateway
 * needs the entries:
 *  { 0x280, 3, -1, 0xD0000,
 *	wd_init, wd_gethead, wd_getpkt, wd_putpkt, wd_stop },
 *  { 0x300, 5, -1, 0xD4000,
 *	wd_init, wd_gethead, wd_getpkt, wd_putpkt, wd_stop },
 */
extern int wd_init();
extern dphead_p wd_gethead();
int wd_getpkt(), wd_putpkt();
extern void wd_stop();
#endif /* NOWD */

#ifndef NONE
/*
 * Novell NE1000/NE2000 board configurations:
 *
 * A system that has one NE adapter with the default factory settings,
 * needs the following entry:
 *
 *  { 0x300, 3, 0,  0x00000,
 *	ne_init, ne_gethead, ne_getpkt, ne_putpkt, ne_stop },
 */
extern int ne_init();
extern dphead_p ne_gethead();
int ne_getpkt(), ne_putpkt();
extern void ne_stop();
#endif /* NONE */

#ifndef NO3C
/*
 * 3Com 503 ethernet board configurations:
 *
 * We support 1 3Com board in a machine.  It should be at I/O address 0x280
 * so as not to clash with the Lance driver which hangs if it pokes around
 * in the memory of the 3Com card.  The shared memory should be enabled.
 * CC000 is fine but any free address is ok.  The number here is ignored
 * and simply taken from the card.
 *  { 0x280, 3, 0, 0xCC000,
 *	e3c_init, e3c_gethead, e3c_getpkt, e3c_putpkt, e3c_stop },
 */
extern int e3c_init();
extern dphead_p e3c_gethead();
int e3c_getpkt(), e3c_putpkt();
extern void e3c_stop();
#endif /* NO3C */

#define NDP8390 (sizeof(dp_info) / sizeof(dpi_t))
dpi_t dp_info[] = {
#ifndef NOWD
    { 0x280, 3, -1, 0xD0000,	/* primary WD adapter */
	wd_init, wd_gethead, wd_getpkt, wd_putpkt, wd_stop },
    { 0x300, 5, -1, 0xD4000,	/* secondary WD adapter */
	wd_init, wd_gethead, wd_getpkt, wd_putpkt, wd_stop },
#endif /* NOWD */
#ifndef NONE
    { 0x280, 3, 0,  0x00000,
	ne_init, ne_gethead, ne_getpkt, ne_putpkt, ne_stop },
    { 0x300, 5, 0,  0x00000,
	ne_init, ne_gethead, ne_getpkt, ne_putpkt, ne_stop },
#endif /* NONE */
#ifndef NO3C
    { 0x280, 3, 0, 0xCC000,	/* 3Com ethernet adapter */
	e3c_init, e3c_gethead, e3c_getpkt, e3c_putpkt, e3c_stop },
#endif /* NO3C */
};

void dp_send();
int dp_alloc(), dp_init(), dp_setmc(), dp_stop();
#endif /* NODP8390 */

/*
 * Ethernet adapter interface list
 */
hei_t heilist[] = {
#ifndef NOLANCE
	{ "lance", NLANCE, la_alloc, la_init, la_send, la_setmc, la_stop },
#endif
#ifndef NODP8390
	{ "dp8390", NDP8390, dp_alloc, dp_init, dp_send, dp_setmc, dp_stop },
#endif
	{ 0, 0, 0, 0, 0, 0, 0 }
};
