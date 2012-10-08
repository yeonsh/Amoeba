/*	@(#)flgrp_hstpro.h	1.3	96/02/27 14:01:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FLGRP_HSTPRO_H__
#define __FLGRP_HSTPRO_H__

/* These can't go in flgrp_hist.h due to circular references
 * to group_p
 */

int	hst_free _ARGS(( group_p ));
hist_p	hst_append _ARGS(( group_p, bchdr_p, char *, f_size_t, int ));
hist_p	hst_lookup (); /* _ARGS(( group_p, uint16 )); */
hist_p	hst_buf_lookup(); /*  _ARGS(( group_p, uint16 )); */
int	hst_store _ARGS(( group_p, bchdr_p, char *, f_size_t, int, int ));
void	hst_buffree _ARGS(( group_p ));

#ifndef SMALL_KERNEL
void	hst_print _ARGS(( group_p ));
#endif

#endif /* __FLGRP_HSTPRO_H__ */
