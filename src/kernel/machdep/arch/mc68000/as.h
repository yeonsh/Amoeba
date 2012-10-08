/*	@(#)as.h	1.3	94/04/06 08:58:37 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
as.h

Created Nov 6, 1991 by Philip Homburg

prototypes for assembler functions in as.S
*/

#ifndef MACHDEP__ARCH__AS_H
#define MACHDEP__ARCH__AS_H

void be_68000 _ARGS(( void ));
void ae_68000 _ARGS(( void ));
void exception _ARGS(( int ));
#ifndef NOPROC
void uawait _ARGS(( void ));
void usuicide _ARGS(( void ));
void unewsys _ARGS(( void ));
void ugetreq _ARGS(( void ));
void uputrep _ARGS(( void ));
void utrans _ARGS(( void ));
void utimeout _ARGS(( void ));
void ucleanup _ARGS(( void ));
void umutrylock _ARGS(( void ));
void umuunlock _ARGS(( void ));
void urpc_getreq _ARGS(( void ));
void urpc_trans _ARGS(( void ));
void urpc_putrep _ARGS(( void ));
#endif /* NOPROC */

long *geta6 _ARGS(( void ));
void trap_to_here _ARGS(( void ));
int call_with_trap _ARGS(( long (*rout) _ARGS(( vir_bytes addr )), long arg,
								long *result ));
long move_tt1 _ARGS(( vir_bytes addr ));
long fnop _ARGS(( vir_bytes addr ));

#endif /* MACHDEP__ARCH__AS_H */
