/*	@(#)host_os.h	1.5	96/02/27 11:54:44 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __HOST_OS_H__
#define __HOST_OS_H__

/*
**	This is the host_os.h file for Unix systems
*/

/*
 * Define ``UNIX'' for processes that are to run under Unix (and for the
 *			Unix kernel)
 * Define ``KERNEL'' when compiling for any kernel.
 * Define ``BSD42'' when compiling for a BSD 4.2 system.
 * Define ``SYS5'' when compiling for a System V system.
 *
 * Defining ``DRIVER'' implies -DUNIX and -DKERNEL.
 * Defining both ``UNIX'' and ``KERNEL'' implies -DDRIVER.
 */

#ifdef SYSV
#ifndef SYS5
#define SYS5	1
#endif
#endif /* SYSV */

#ifdef DRIVER

#ifndef UNIX
#define UNIX 1
#endif /* UNIX */

#ifndef KERNEL
#define KERNEL 1
#endif /* KERNEL */

#endif /* DRIVER */

#ifdef UNIX

#ifdef KERNEL

#ifndef DRIVER
#define DRIVER
#endif /* DRIVER */

#define BUFFERED

#ifdef SMALL		/* some systems define SMALL in param.h */
#define SmAlL		/* so we have to kludge around it */
#endif /* SMALL */

#include "param.h"

#ifndef SmAlL
#undef SMALL
#endif /* SmAlL */
#undef SmAlL

#ifdef SYS5
#include "types.h"
#endif /* SYS5 */

#include "dir.h"

#ifndef NSIG
#include "signal.h"
#endif /* NSIG */

#ifndef EPERM
#include "errno.h"
#endif /* EPERM */

#include "user.h"
#include "proc.h"
#include "systm.h"

#ifdef BSD42
#ifndef IRIX33
#include "kernel.h"
#endif /* IRIX33 */
#include "ioctl.h"
#endif /* BSD42 */

#include "buf.h"

#if defined(SYS5) || defined(IRIX)
#define u_return	u_r.r_reg.r_val1
#else
#define u_return	u_r.r_val1
#endif /* SYS5 */

#ifndef HZ
#define HZ		hz
#endif /* HZ */

/* The following sequences of undefs and defines is to avoid clashes in the
 * naming of variables and constants in Amoeba and UNIX.
 */

#undef ABORT
#undef ABORTED	
#undef ACK
#undef ACKED
#undef ALIVE
#undef BADADDRESS
#undef BROADCAST
#undef BUFSIZE
#undef CRASH
#undef DEAD
#undef DELETE
#undef DONE
#undef DONTKNOW
#undef ENQUIRY
#undef FAIL
#undef FAILED
#undef GLOBAL
#undef HASHMASK
#undef HEADERSIZE
#undef HERE
#undef IDLE
#undef IMMORTAL
#undef LAST
#undef LOCAL
#undef LOCATE
#undef LOCATING
#undef LOOK
#undef MEMFAULT
#undef MORTAL
#undef NAK
#undef NESTED
#undef NHASH
#undef NILVECTOR
#undef NOSEND
#undef NOTFOUND
#undef NOWAIT
#undef NOWHERE
#undef PACKETSIZE
#undef PORT
#undef RECEIVING
#undef REPLY
#undef REQUEST
#undef RETRANS
#undef RUNNABLE
#undef SEND
#undef SENDING
#undef SERVING
#undef SOMEWHERE
#undef THREAD
#undef TYPE
#undef WAIT

#undef bit
#undef concat
#undef disable
#undef enable
#undef hash
#undef hibyte
#undef lobyte
#undef siteaddr
#undef sizeoftable

#undef timeout
#undef trans
#undef getreq
#undef putrep

#define allocbuf	am_allocbuf
#define append		name_append
#define area		am_area
#define badassertion	am_badassertion
#define cleanup		am_cleanup
#define debug		am_debug
#define destroy		am_destroy
#define freebuf		am_freebuf
#define getall		am_gall
#define getbuf		am_gbuf
#define getreq		am_greq
#define getsig		am_gsig
#define handle		am_handle
#define locate		am_locate
#define netenable	am_netenable
#define netsweep	am_sweep
#define totthread		am_ntsk
#define porttab		am_ptab
#define puthead		am_puthead
#define putbuf		am_pbuf
#define putrep		am_prep
#define putsig		am_psig
#define sendsig		am_sendsig
#define sleep		am_sleep
#define thread		am_task
#define ticker		am_ticker
#define timeout		am_timeout
#define trans		am_trans
#define uniqport	am_uniqport
#define upperthread	am_uppertask
#define wakeup		am_wakeup

/**********************************************************************/

#ifdef ULTRIX_PMAX
#define disable		splhigh
#else
#define disable		spl6
#endif /* ULTRIX_PMAX */
#define enable		am_enable

typedef caddr_t vir_bytes;
typedef caddr_t phys_bytes;

#else /*!KERNEL*/

#if defined(__SVR4)
#include <sys/ioccom.h>
#endif

#include <sys/ioctl.h>

#endif /*KERNEL*/

#endif /*UNIX*/

/* Actually this define is for Unix, but now you don't *have* to define UNIX */
#define SIGAMOEBA	SIGEMT

#endif /* __HOST_OS_H__ */
