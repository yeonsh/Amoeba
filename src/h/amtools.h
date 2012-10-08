/*	@(#)amtools.h	1.7	96/02/27 10:24:58 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __AMTOOLS_H__
#define __AMTOOLS_H__

/* Frequently used includes */
#include <amoeba.h>
#include <cmdreg.h>
#include <stdcom.h>
#include <stderr.h>
#include <file.h>

/* Function definitions */
#include <module/name.h>
#include <module/direct.h>
#include <module/mutex.h>
#include <module/prv.h>
#include <module/stdcmd.h>
#include <module/ar.h>
#include <module/syscall.h>

/* Frequently used C includes */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/* Random useful things */
#define NILCAP ((capability *)NULL)
#define NILPD ((process_d *)NULL)
#define TICKSPERSEC 1000 /* Amoeba timer units (mu_trylock, sys_milli) */

#ifndef CAPZERO
#define CAPZERO(cap) (void) memset((_VOIDSTAR)(cap), 0, (size_t) CAPSIZE)
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* Getopt interface */
extern char *optarg;
extern int optind;

#endif /* __AMTOOLS_H__ */
