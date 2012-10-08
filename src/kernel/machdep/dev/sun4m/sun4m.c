/*	@(#)sun4m.c	1.4	96/02/27 13:58:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "promid_sun4.h"
#include "openprom.h"
#include "memaccess.h"
#include "sun4m_auxio.h"
#include "assert.h"
INIT_ASSERT

struct mach_info machine;
struct openboot *prom_devp;

find_machine(wim)
int wim;
{
	int numwins;

	/* HACK for now */
	machine.mi_id = 0x42;	/* Who knows ? */
	for (numwins = 0; wim != 0; numwins++)
	    wim >>= 1;
	machine.mi_nwindows = numwins;
	if (**prom_devp->op_bootargs == '-') {
		switch(prom_devp->op_bootargs[0][1]) {
		case 'b' : (*prom_devp->op_enter)();
		}
	}
}

/*ARGSUSED*/
enter_monitor(begin, end)
char *begin, *end;
{

	(*prom_devp->op_enter)();
	return 0;
}

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

/*
 * get_console_type - called from inituart to determine console device.
 */

#define	MXPTH	100

int
get_console_type()
{
    int		proplen;
    uint8	stdin_path[MXPTH];

    proplen = obp_getattr(0, "stdin-path", (void *) stdin_path, MXPTH);
    if (proplen == -1)
	panic("inituart: can't find `stdin-path' attribute of CPU\n");
    assert(proplen < MXPTH);
    stdin_path[proplen] = '\0';
    if (stdin_path[proplen-3] == ':')
    {
	if (stdin_path[proplen-2] == 'a')
	    return OP_IUARTA;
	if (stdin_path[proplen-2] == 'b')
	    return OP_IUARTB;
    }
    else
    {
	if (stdin_path[proplen-2] == '0')
	    return OP_IKEYBD;
    }
    panic("inituart: can't parse stdin-path");
    /*NOTREACHED*/
}

/*
 * Routines to manage the auxillary I/O register #1 (Floppy and LED)
 */

static volatile unsigned char *	Auxio_reg;

static void
set_auxio_addr(p)
nodelist *	p;
{
    int	proplen;

    proplen = obp_getattr(p->l_curnode, "address", (void *) &Auxio_reg,
							sizeof (Auxio_reg));
    compare(proplen, ==, sizeof (Auxio_reg));
}

void
init_auxioreg()
{
    obp_regnode("name", "auxio", set_auxio_addr);
}

void
set_auxioreg(bits)
unsigned int bits;
{
    assert(Auxio_reg);
    *Auxio_reg = (unsigned char) bits;
}

unsigned int
get_auxioreg()
{
    assert(Auxio_reg);
    return *Auxio_reg;
}

void
switch_off_led()
{
    unsigned int tmp;

    tmp = get_auxioreg();
    tmp &= ~AUX_LED;
    set_auxioreg(tmp);
}

void
switch_on_led()
{
    unsigned int tmp;

    tmp = get_auxioreg();
    tmp |= AUX_LED;
    set_auxioreg(tmp);
}
