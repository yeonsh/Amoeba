/*	@(#)config.c	1.3	96/02/27 10:12:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Configuration File
 *
 *	This file provides the means for configuring the various supported
 *	host operating system disk structures.  This allows Amoeba to
 *	cohabit disks with other operating systems.
 *
 *	To add a new OS type add an entry in the "host_os_menu", add a file
 *	containing the new support routines as required by the struct host_dep
 *	(see host_dep.h) and add a struct host_dep to the array Host_p.
 *	Because it is obvious it must also be said that the array Host_p must
 *	have its entries in the same order as the host_os_menu array.
 *
 *	To add a new type of disk to the disktype table just give its name
 *	and geometry.
 *
 * Author: Gregory J. Sharp, June 1992
 */

#include "amoeba.h"
#include "menu.h"
#include "module/disk.h"
#include "vdisk/disklabel.h"
#include "vdisk/disktype.h"
#include "host_dep.h"


void
noop _ARGS(( void ))
{
}

struct menu	Host_os_menu[] = {
	{ "Dos",	noop },
	{ "SunOS",	noop },
	{ 0,		0 }
};

extern host_dep	dos_host_dep;
extern host_dep	sunos_host_dep;

host_dep *	Host_p[] = {
	&dos_host_dep,
	&sunos_host_dep
};

int Host_psize = sizeof (Host_p) / sizeof (host_dep *);


/*
 * The following table is used to initialise the disk geometry for well
 * known disks.  The "Other" is for unknown disks.  It is initialised to
 * -1 so that the system knows to insist on the user giving a value.
 * NB: it initialises Amoeba geometry information.  For other OS labels
 *     other geometry defaults may be required.
 */


dsktype	Dtypetab[] =
{
  /*      name                  bpt,   bts,  #cyls,  alts, heads, sects */
  { "CDC Wren IV 94171-307",   20833,    0,   1410,     2,     9,    46 },
  { "CDC Wren IV 94171-344",   20833,    0,   1545,     4,     9,    46 },
  { "CDC Wren V 94181-385",    32300,    0,    789,     2,    15,    55 },
  { "CDC Wren V 94181-702",        0,    0,    716,     0,    15,   109 },
  { "CDC Wren VII 94601-12G",  41301,	 0,   1925,     2,    15,    70 },
  { "Fujitsu M2243AS",         10416,    0,    752,     2,    11,    17 },
  { "Fujitsu M2266SA",	       46635,	 0,   1642,	2,    15,    85 },
  { "Micropolis 1325",         10416,    0,   1022,     2,     8,    17 },
  { "Other",		          -1,   -1,     -1,    -1,    -1,    -1 },
};

int	Dtypetabsize = SIZEOFTABLE(Dtypetab);
