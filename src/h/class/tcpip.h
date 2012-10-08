/*	@(#)tcpip.h	1.2	94/04/06 15:54:24 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __TCPIP_H__
#define __TCPIP_H__

/* Inherited classes: */
#include "stdinfo.h"

/* Constants: */
#define TCPR_LSN 2
#define TCPR_CON 4
#define TCPR_PCON 8

typedef unsigned char host_t[4];

/* Operators: */
extern errstat tcpip_listen();
extern errstat tcpip_newlisten();
extern errstat tcpip_connect();
extern errstat tcpip_pconnect();
extern errstat tcpip_stat();
extern errstat tcpip_lookup();

#endif /* __TCPIP_H__ */
