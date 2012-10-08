/*	@(#)ux_int.c	1.5	96/02/27 11:53:29 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * HACK ALERT:
 * We want to use two modules with a name clash: amoeba.h - defines timeout()
 * (a function we don't want to use here) and the unix kernel which has a
 * sweeper system with a function timeout() which we do want to use.  Thus
 * we must stop amoeba.h defining anything to do with timeout.
 */

#define ail /* to get rid of the amoeba function prototypes */

#include "amoeba.h"
#undef timeout

#undef ail

#include "sys/flip/flip.h"
#include "sys/flip/packet.h"
#include "ux_int.h"
#include "ux_rpc_int.h"
#include "ux_ip_int.h"
#include "ux_ctrl_int.h"
#include "kthread.h"
#include "sys/flip/flrpc_port.h"
#include "sys/flip/flrpc.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioccom.h>
#include <time.h>
#include <signal.h>
#include <server/process/proc.h>
#include <sys/user.h>
#include <sys/kernel.h>
#include <assert.h>
INIT_ASSERT

extern void sweeper_run();

static long seed;

uint16
rand()
{
    return (uint16) (seed = seed * 1103515245 + 12345);
}


rnd_init()
{
    seed = time.tv_sec;
}


/* The flip interface wants to sleep. For example, to wait until
 * until a flip address is located. Return 1, if the sleep is terminated
 * due to signal. (see interface.c)
 */
int uxint_await(event, val)
    char *event;
    long val;
{
    assert(val == 0);
    if(setjmp(&u.u_qsave)) {
        return(1);
    }

    sleep((caddr_t) event, PZERO + 1);
    return(0);
}


/* Wake the process that is sleeping in the flip interface up. */
int uxint_wakeup(event)
    char *event;
{
    wakeup((caddr_t) event);
}


static int ticks_per_sweep;


/* Run all the registered timers in the flip box. */
fl_timer()
{
    register x;

    x = spl6();
    sweeper_run(SWEEPINTERVAL);
    splx(x);
    timeout(fl_timer, (caddr_t) 0, ticks_per_sweep);
}



unix_init()
{
    void adr_init();
    void port_init();
    void netswitch_init();
    void rpc_init();
    void kid_init();
    void int_init();

    rnd_init();
    adr_init();
    netswitch_init();
    fleth_init();
    int_init();
    rpc_init();
    port_init();
    kid_init();
    ticks_per_sweep=hz/10;
    timeout(fl_timer, (caddr_t) 0, ticks_per_sweep);
}


/*ARGSUSED*/
flopen(dev, flags)
    dev_t dev;
{
    static initstate;
    int r;
    register x;
    
    x = spl6();
    switch (initstate) {
    case 0:
	initstate = 1;
	unix_init();
	initstate = 2;
	wakeup((caddr_t) &initstate);
	break;
    case 1:
	do {
	    sleep((caddr_t) &initstate, PSLEP + 1);
	} while (initstate == 1);
    }
    /* Call open routines. */
    if(minor(dev) >= FIRST_RPC && minor(dev) <= LAST_RPC)  {
	r = flrpc_open(dev, flags);
    } else if(minor(dev) >= FIRST_IP && minor(dev) <= LAST_IP)  {
	r = fl_ip_open(dev, flags);
    } else if(minor(dev) >= FIRST_CTRL && minor(dev) <= LAST_CTRL) {
	r = flctrl_open(dev, flags);
    } else r = ENXIO;
    splx(x);
    return(r);
}


/*ARGSUSED*/
flioctl(dev, cmd, fa, flag)
    dev_t dev;
    void *fa;
{
    int r;
    register x;

    x = spl6();
    if(minor(dev) >= FIRST_RPC && minor(dev) <= LAST_RPC) {
	r = flrpc_ioctl(dev, cmd, (struct rpc_args *) fa, flag);
    } else if(minor(dev) >= FIRST_IP && minor(dev) <= LAST_IP) {
	r = fl_ip_ioctl(dev, cmd, (struct ip_args *) fa, flag);
    } else if(minor(dev) >= FIRST_CTRL && minor(dev) <= LAST_CTRL) {
	r = flctrl_ioctl(dev, cmd, (struct ctrl_args *) fa, flag);
    } else r = ENXIO;
    splx(x);
    return(r);
}


/*ARGSUSED*/
flclose(dev, flags)
    dev_t dev;
{
    int r;
    register x;
    /* Call close routines. */

    x = spl6();
    if(minor(dev) >= FIRST_RPC && minor(dev) <= LAST_RPC) {
	r =  flrpc_close(dev, flags);
    } else if(minor(dev) >= FIRST_IP && minor(dev) <= LAST_IP) {
	r =  fl_ip_close(dev, flags);
    } else if(minor(dev) >= FIRST_CTRL && minor(dev) <= LAST_CTRL) {
	r =  flctrl_close(dev, flags);
    } else {
	r =  ENXIO;
    }
    splx(x);
    return(r);
}
