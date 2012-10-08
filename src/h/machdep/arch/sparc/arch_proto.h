/*	@(#)arch_proto.h	1.2	94/04/06 16:30:49 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MACHDEP__ARCH__ARCH_PROTO_H__
#define __MACHDEP__ARCH__ARCH_PROTO_H__

/*
 * This file contains architecture depend declarations and prototypes
 * for the sparc processor family.
 */

#ifdef PROFILE
void profile_timer _ARGS(( thread_ustate *tp, uint32  pc, uint32  psr ));
errstat profile_start _ARGS(( char *prof_buffer, int prof_size, long milli,
			     void (*prof_done) (int res) ));
void profile_stop _ARGS(( void ));
#endif /* PROFILE */

#endif /* __MACHDEP__ARCH__ARCH_PROTO_H__ */
