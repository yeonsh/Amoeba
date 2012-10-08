/*	@(#)ux_ctrl_int.h	1.5	96/02/27 11:55:01 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __UX_CTRL_INT_H__
#define __UX_CTRL_INT_H__

#ifdef __SVR4
/*
 * If we have a system 5 version then we probably need streams-based, rather
 * than ioctl-based communication with the kernel.
 */
#define	STREAMS		1
#endif /* __SVR4 */

#ifdef __STDC__
#include "ioctlfix.h"

#define F_STATISTICS	_IOWR('f', FLCTRL_STAT, struct ctrl_args)
#define F_DUMP		_IOWR('f', FLCTRL_DUMP, struct ctrl_args)
#define F_CONTROL	_IOWR('f', FLCTRL_CTRL, struct ctrl_args)
#else
#define F_STATISTICS	_IOWR(f, FLCTRL_STAT, struct ctrl_args)
#define F_DUMP		_IOWR(f, FLCTRL_DUMP, struct ctrl_args)
#define F_CONTROL	_IOWR(f, FLCTRL_CTRL, struct ctrl_args)

#endif /* __STDC__ */

#define MAXPRINTBUF	30000

struct ctrl_args {
#ifdef STREAMS
    int		ctrl_type;
#endif /* STREAMS */
    int		ctrl_status;
    char 	*ctrl_buffer;
    short 	ctrl_nw;	
    short	ctrl_delay;
    short	ctrl_loss;
    short	ctrl_on;
    short	ctrl_trusted;
};

#endif /* __UX_CTRL_INT_H__ */
