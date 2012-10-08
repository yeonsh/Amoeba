/*	@(#)table.h	1.4	96/02/27 10:39:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __TABLE_H__
#define __TABLE_H__

#ifndef SMALL_KERNEL

#define ESCAPE		'\036'

struct dumptab {
    char	d_char;
    int		(*d_func)();
    char	*d_help;
    rights_bits	d_req_rgts;
};
#endif

#endif /* __TABLE_H__ */
