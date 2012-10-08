/*	@(#)scc2698.h	1.2	94/04/06 16:45:35 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SCC2698_H__
#define __SCC2698_H__

/*
 * Defines for Signetics Octal Universal Asynchronous Receiver/Transmitter
 *
 * Some quick and dirty get the octart running info.
 *
 * Only using the clock on the tp22v now
 */

struct scc2698quart {
	char	mr1x;		/* or mr2x */
	char	csrx;
	char	crx;
	char	thrx;
	char	acrX;
	char	imrX;
	char	cturX;
	char	ctlrX;
	char	mr1y;		/* or mr2y */
	char	csry;
	char	cry;
	char	thry;
	char	res1;
	char	opcrX;
	char	rd_StartCTX;
	char	rd_StopCTX;
};

#endif /* __SCC2698_H__ */
