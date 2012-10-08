/*	@(#)transparam.c	1.2	94/04/06 08:35:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "global.h"

/************************************************************************/
/*      TRANS                                                           */
/************************************************************************/

#ifndef NONET

#define MINLOCCNT          2    /* locate message sent every dsec */
#define MAXLOCCNT        200    /* locate message sent every MAXLOCCNT dsec */

#define RETRANSTIME        5    /* retransmission time in dsec */
#define CRASHTIME        100    /* crash timer in dsec */
#define CLIENTCRASH      500    /* client must probe within this time */

#define MAXRETRANS        20    /* max. number of transmissions */
#define MINCRASH           5    /* enquiry sent MINCRASH times during recv */
#define MAXCRASH          20    /* enquiry sent MAXCRASH times during serv */

uint16 minloccnt        = MINLOCCNT;
uint16 maxloccnt        = MAXLOCCNT;

uint16 retranstime      = RETRANSTIME;
uint16 crashtime        = CRASHTIME;
uint16 clientcrash      = CLIENTCRASH;

uint16 maxretrans       = MAXRETRANS;
uint16 mincrash         = MINCRASH;
uint16 maxcrash         = MAXCRASH;

#endif /* NONET */
