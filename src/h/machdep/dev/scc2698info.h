/*	@(#)scc2698info.h	1.2	94/04/06 16:45:42 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SCC2698INFO_H__
#define __SCC2698INFO_H__

/*
 * parameters for Signetics SCC2698 Octart
 */

extern
struct scc2698info {
	vir_bytes scci_devaddr;
	short     scci_vector;
} scc2698info;

#endif /* __SCC2698INFO_H__ */
