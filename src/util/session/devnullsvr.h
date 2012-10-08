/*	@(#)devnullsvr.h	1.2	94/04/07 16:02:52 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef DEVNULL_H
#define DEVNULL_H

errstat publish_devnullcap   _ARGS((void));
errstat start_devnullsvr     _ARGS((void));
errstat unpublish_devnullcap _ARGS((void));

#endif /* DEVNULL_H */
