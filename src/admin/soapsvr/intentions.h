/*	@(#)intentions.h	1.4	96/02/27 10:22:20 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef INTENTIONS_H
#define INTENTIONS_H

#include "_ARGS.h"
#include "soap/super.h"

extern void intent_init		  _ARGS((void));
extern void intent_add		  _ARGS((objnum, capability[]));
extern int  intent_avail	  _ARGS((void));
extern void intent_clear	  _ARGS((void));
extern void intent_rollforward	  _ARGS((void));
extern void intent_check	  _ARGS((int forced));
extern void intent_watchdog	  _ARGS((char *param, int psize));
extern void intent_make_permanent _ARGS((void));
extern void intent_undo		  _ARGS((void));

extern errstat sp_commit	_ARGS((void));
extern int     sp_gotintent     _ARGS((header *hdr, char *buf, int size));

#endif
