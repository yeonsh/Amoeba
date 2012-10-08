/*	@(#)sun4c.c	1.3	94/04/06 09:31:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "memaccess.h"
#include "promid_sun4.h"
#include "sun4c_auxio.h"
#include "global.h"
#include "map.h"
#include "debug.h"
#include "openprom.h"
#include "mmuproto.h"


/* Important global variables for the sun4c */
struct mach_info machine;       	/* Our machine information */
struct openboot *prom_devp;		/* Prom interface pointer */

/*
 * List of possible CPU types, with the first one being the default
 */
struct mach_info mach_info[] = {
    { MACH_SUN4C_60, 7, 8, 128,  5 },	/* SS1  */
    { MACH_SUN4C_40, 7, 8, 128,  8 },	/* IPC  */
    { MACH_SUN4C_65, 7, 8, 128,  8 },	/* SS1+ */
    { MACH_SUN4C_20, 7, 8, 128,  5 },	/* SLC  */
    { MACH_SUN4C_75, 8,16, 256, 12 },	/* SS2  */
    { MACH_SUN4C_30, 8,16, 256, 11 },	/* ELC  */
    { MACH_SUN4C_50, 8,16, 256, 12 },	/* IPX  */
    { 0 }
};

#ifndef NOTTY

#define NTTY            3                       /* number of terminals */
#define NTTYTASK        (2*NTTY)

uint16 ntty	  = NTTY;
uint16 nttythread = NTTYTASK;


#include "tty/tty.h"

/* Initial speeds of the ttys */
short tty_speeds[] = {
	B_9600,
	B_9600,
	B_9600
};

#endif /* !NOTTY */


static volatile unsigned char *Auxio_reg;


void
microsec_delay(n)
int	n;
{
    int	delay;
    int	x;

    delay = machine.mi_cpu_delay;
    do
    {
	x = delay;
	while (--x != 0)
	    /*nothing */;
    } while (--n >= 0);
}


/*ARGSUSED*/
enter_monitor(begin, end)
char *begin, *end;
{
    (*prom_devp->op_enter)();
    return 0;
}


void
init_auxioreg()
{
    /*
     * Old versions of the prom return the wrong address.
     * Just hack on the low order two bits and all will be fine.
     */
    Auxio_reg = (unsigned char *) (mmu_virtaddr("/auxiliary-io") | 3);
}


void
set_auxioreg(bits, state)
unsigned int	bits;	/* bits of Auxio register to set */
int		state;	/* 0 = off, 1 = on */
{
    if (state == 0) /* turn off bits! */
    {
	unsigned char tmp;

	tmp = *Auxio_reg;
	tmp &= ~bits;
	tmp |= AUXIO_MUST_BE_ON;
	*Auxio_reg = tmp;
    }
    else /* Turn on bits */
    {
	*Auxio_reg |= (bits | AUXIO_MUST_BE_ON);
    }
}


uint32
get_auxioreg()
{
    return (uint32) *Auxio_reg;
}


/*
 * switch_on/off_led() -- switch on/off the little green led on the front of
 * the sparc station which everyone thinks is the power light.
 */
void
switch_on_led()
{
    set_auxioreg(AUXIO_LED, 1);
}


void
switch_off_led()
{
    set_auxioreg(AUXIO_LED, 0);
}


#define IDPROM_SIZE 32

/*
 * find_machine() -- determine processor type and fill in the 'machine'
 * structure, allowing others to learn what many consider constant.
 */
void
find_machine()
{
    struct mach_info *machp;
    unsigned char idprom[IDPROM_SIZE];
    int prom_id;
    unsigned int value;

    if (obp_getattr(0, "idprom", (void *) idprom, IDPROM_SIZE) != IDPROM_SIZE)
	panic("find_machine: no idprom info in devtree");
    prom_id = idprom[1];
    DPRINTF(2, ("Found promid %x\n", prom_id));
    for ( machp = mach_info; machp->mi_id != 0; machp++ )
	if ( machp->mi_id == prom_id ) break;

    if ( machp->mi_id == 0 )	/* Use default (first element) */
	machp = mach_info;

    /*
     * We make a copy of this for two reasons: (1) might want to modify it
     * (to perhaps trim extra PMEGs or something), and (2) it works better to
     * have machine.mi_xxx rather than machp->mi_xxx, since it only requires
     * one memory reference to load it.
     */
    machine = *machp;

    /*
     * Now try to fill in stuff from the openprom that may differ from the
     * expected initial values for particular types of machine.
     */

    /* Put in the "real value" of the machine id. */
    machine.mi_id = prom_id;
    /* Number of mmu contexts */
    if (obp_getattr(0, "mmu-nctx", (void *) &value, sizeof value) == sizeof value)
    {
	DPRINTF(2, ("got #contexts from openprom (%d)\n", value));
	machine.mi_contexts = value;
    }
    /* Number of pmegs */
    if (obp_getattr(0, "mmu-npmg", (void *) &value, sizeof value) == sizeof value)
    {
	DPRINTF(2, ("got #pmegs from openprom (%d)\n", value));
	machine.mi_pmegs = value;
    }

    DPRINTF(2, ("find_machine %d windows\n", machine.mi_nwindows));
}


/*
 * get_console_type - called by inituart to find out the console type.
 *	This is here since the uart driver is shared with the sun4m and it
 *	has a different way of finding its console
 */

int
get_console_type()
{
    return *prom_devp->v1_input;
}
