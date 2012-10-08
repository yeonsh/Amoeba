/*	@(#)arch_proto.h	1.4	94/04/06 16:00:13 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
arch_proto.h

Created Nov 7, 1991 by Philip Homburg

This file contains architecture depend declarations and prototypes for the
mc68000 processor family.
*/

#ifndef __MACHDEP__ARCH__ARCH_PROTO_H__
#define __MACHDEP__ARCH__ARCH_PROTO_H__

struct stframe 
{
	long st_vecoff;
	long st_reg[4];
#ifdef __ACK /* ACK compiler */
	long st_extra_reg;
#endif
	long st_return;
	uint16 st_sr;
	uint16 st_pc_high;
	uint16 st_pc_low;
};

int setvec _ARGS(( int vecno, void (*rout)(int) ));
#if PROFILE
void profile_timer _ARGS(( struct stframe sp ));
errstat profile_start _ARGS(( char *prof_buffer, int /* bufsize */ prof_size,
		long milli, void (*prof_done) (int /* bufsize */ res) ));
void profile_stop _ARGS(( void ));
#endif

#endif /* __MACHDEP__ARCH__ARCH_PROTO_H__ */
