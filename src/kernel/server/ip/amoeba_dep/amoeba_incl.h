/*	@(#)amoeba_incl.h	1.3	96/02/27 14:13:57 */
/*
amoeba_incl.h
*/

#ifndef AMOEBA_DEP__AMOEBA_INCL_H
#define AMOEBA_DEP__AMOEBA_INCL_H

#define port am_port_t	/* Amoeba headers need these. */
#define header am_header_t
#define bufsize am_bufsize_t

#include <amtools.h>
#include <cmdreg.h>
#include <fault.h>
#include <semaphore.h>
#include <stdcom.h>
#include <stderr.h>
/* #include <thread.h> */
#include <unistd.h>

#include <module/buffers.h>
#include <module/mutex.h>
#include <module/name.h>
#include <module/prv.h>
#include <module/rnd.h>
#include <module/signals.h>

#include <server/ip/tcpip.h>

#include <sys/proto.h>

#if AM_KERNEL
#include <capset.h>
#include <kerneldir.h>
#include <soap/soap.h>
#else
#include <server/ehook/ehook.h>
#endif

#undef port
#undef header
#undef bufsize

#if AM_KERNEL
#define _sys_milli getmilli
#endif

void new_thread _ARGS(( void (*func)(void), size_t stacksiz ));
void append_name _ARGS(( char *name, capability *cap ));

extern mutex mu_generic;	/* unsures single threaded generic code */

/* Prototypes for some library functions */

char *bprintf _ARGS(( char *begin, char *end, char *fmt, ... ));

#endif /* AMOEBA_DEP__AMOEBA_INCL_H */
