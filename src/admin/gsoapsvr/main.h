/*	@(#)main.h	1.1	96/02/27 10:02:43 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#ifndef MAIN_H
#define MAIN_H

/*
 * Routines to (try to) lock and unlock global mutex
 */
void sp_begin	_ARGS((void));
int  sp_try	_ARGS((void));
int  for_me	_ARGS((header *hdr));
void sp_end	_ARGS((void));

/*
 * fully_initialised() tells whether we have an up to date super file,
 * and initialised sp_table.
 */
int fully_initialised _ARGS((void));

#include "diag.h"
#include "grpmain.h"

#endif
