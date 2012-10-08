/*	@(#)devproc.h	1.2	94/04/07 16:03:15 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef DEVPROC_H
#define DEVPROC_H

errstat start_devprocsvr     _ARGS((void));
errstat publish_devproccap   _ARGS((void));
errstat unpublish_devproccap _ARGS((void));

errstat proc_publish   _ARGS((int pid, int replace, capability *proccap));
errstat proc_unpublish _ARGS((int pid));

void init_dirsvr _ARGS((void));

#endif
