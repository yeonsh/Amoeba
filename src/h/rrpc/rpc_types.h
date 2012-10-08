/*	@(#)rpc_types.h	1.2	93/10/08 11:53:43 */
/*
 *
 * Replicated RPC Package
 * Copyright 1992 Mark D. Wood
 * Vrije Universiteit, The Netherlands
 *
 * Previous versions
 * Copyright 1990, 1991 by Mark D. Wood and the Meta Project, Cornell University
 *
 * Rights to use this source in unmodified form granted for all
 * commercial and research uses.  Rights to develop derivative
 * versions reserved by the authors.
 *
 * RPC type includes
 */

#include <amoeba.h>
#include <cmdreg.h>
#include <module/rnd.h>
#include <module/name.h>
#include <module/buffers.h>
#include <semaphore.h>

#ifdef __STDC__
#include <stddef.h>
#endif

#include <stderr.h>
#include <stdio.h>

#define TYPE_BASE 0xf
#define TYPE_INT 0
#define TYPE_REAL 1
#define TYPE_STRING 2
#define TYPE_ADDRESS 4

typedef void * PTR;

typedef port address;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifdef DEBUG
#define TRACE_MSGS 1
#define TRACE_GRD 2
#define TRACE_EXEC 4
#define TRACE_EVAL 8
#define TRACE_SUBS 16
#define TRACE_REP 32
#define TRACE_MUTEX 64
#define TRACE_GRP 128
#define TRACE_THREADS 256

extern int msg_trace;
#endif

