/*	@(#)sgtty.h	1.3	94/04/06 16:54:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SGTTY_H__
#define __SGTTY_H__

#include "sys/ioctl.h"

int gtty _ARGS(( int _fd, struct sgttyb *_argp ));
int stty _ARGS(( int _fd, struct sgttyb *_argp ));

#endif /* __SGTTY_H__ */
