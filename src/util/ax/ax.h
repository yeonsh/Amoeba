/*	@(#)ax.h	1.1	96/02/27 13:07:25 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __AX_H__
#define	__AX_H__

struct tty_caps {
    capability	t_cap;	/* cap for the tty */
    int		t_fd;	/* real fd for the tty */
};

#endif /* __AX_H__ */
