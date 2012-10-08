/*	@(#)ux_ctrl_int.c	1.5	96/02/27 11:53:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioccom.h>
#include <sys/user.h>
#include <assert.h>
INIT_ASSERT

#include "ux_int.h"
#include "ux_ctrl_int.h"

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"

struct ctrl_device {
    char	ctrl_buf[MAXPRINTBUF];
};

static struct ctrl_device ctrl_dev[NCTRL]; 	/* devices for ctrl */

static void flctrl_stat(fd, fa)
    struct ctrl_device *fd;
    struct ctrl_args *fa;
{
    int sum;
    
    sum = rpc_statistics(fd->ctrl_buf, fd->ctrl_buf + MAXPRINTBUF);
    sum += int_statistics(fd->ctrl_buf + sum, fd->ctrl_buf + MAXPRINTBUF);
    sum += flip_netstat(fd->ctrl_buf + sum, fd->ctrl_buf + MAXPRINTBUF);
    sum += ffstatistics(fd->ctrl_buf + sum, fd->ctrl_buf + MAXPRINTBUF);
    sum += fleth_stat(fd->ctrl_buf + sum, fd->ctrl_buf + MAXPRINTBUF);
    if(copyout(fd->ctrl_buf, fa->ctrl_buffer, sum) != 0) {
	fa->ctrl_status = STD_SYSERR;
	return;
    }
    fa->ctrl_status = sum;
}


static void flctrl_dump(fd, fa)
    struct ctrl_device *fd;
    struct ctrl_args *fa;
{
    int sum;
    
    sum = rpc_dump(fd->ctrl_buf, fd->ctrl_buf + MAXPRINTBUF);
    sum += port_dump(fd->ctrl_buf + sum, fd->ctrl_buf + MAXPRINTBUF);
    sum += kid_dump(fd->ctrl_buf + sum, fd->ctrl_buf + MAXPRINTBUF);
    sum += flip_netdump(fd->ctrl_buf + sum, fd->ctrl_buf + MAXPRINTBUF);
    sum += int_dump(fd->ctrl_buf + sum, fd->ctrl_buf + MAXPRINTBUF);
    sum += adr_dump(fd->ctrl_buf + sum, fd->ctrl_buf + MAXPRINTBUF);
    sum += ff_dump(fd->ctrl_buf + sum, fd->ctrl_buf + MAXPRINTBUF);
    if(copyout(fd->ctrl_buf, fa->ctrl_buffer, sum) != 0) {
	fa->ctrl_status = STD_SYSERR;
	return;
    }
    fa->ctrl_status = sum;
}


#ifdef FLNETCONTROL

static void flctrl_ctrl(fa)
    struct ctrl_args *fa;
{
    short nw, delay, loss, on, trusted;
    
    nw= fa->ctrl_nw;
    on= fa->ctrl_on;
    loss= fa->ctrl_loss;
    delay= fa->ctrl_delay;
    trusted = fa->ctrl_trusted;
    
    if(flip_control(nw, &delay, &loss, &on, &trusted))
    {
	fa->ctrl_nw = nw;
	fa->ctrl_on = on;
	fa->ctrl_loss = loss;
	fa->ctrl_delay = delay;
	fa->ctrl_trusted = trusted;
	fa->ctrl_status = STD_OK;
    } else fa->ctrl_status = STD_SYSERR;
}
#endif /* FLNETCONTROL */


/*ARGSUSED*/
int flctrl_ioctl(dev, cmd, fa, flag)
    dev_t dev;
    struct ctrl_args *fa;
{
    struct ctrl_device *fd;
    
    assert(minor(dev)-FIRST_CTRL >= 0 && minor(dev)-FIRST_CTRL < NCTRL);
    fd = &ctrl_dev[minor(dev)-FIRST_CTRL];
    switch (cmd) {
    case F_STATISTICS:
	flctrl_stat(fd, fa);
	break;
    case F_DUMP:
	flctrl_dump(fd, fa);
	break;
#ifdef FLNETCONTROL
    case F_CONTROL:
	flctrl_ctrl(fa);
	break;
#endif /* FLNETCONTROL */
    default:
	return ENXIO;
    }
    return 0;
}


/*ARGSUSED*/
int flctrl_open(dev, flags)
    dev_t dev;
    int flags;
{
    return(0);
}


/*ARGSUSED*/
int flctrl_close(dev, flags)
    dev_t dev;
    int flags;
{
    return(0);
}
