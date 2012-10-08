/*	@(#)trap.h	1.2	94/04/06 09:00:04 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
trap.h

Created Nov 6, 1991 by Philip Homburg
*/

#ifndef MACHDEP__ARCH__TRAP_H
#define MACHDEP__ARCH__TRAP_H

void swtrap _ARGS(( signum signo, int reason, thread_ustate **framepp ));
void trap _ARGS(( int /* uint16 */ reason, thread_ustate **fault ));

#endif /* MACHDEP__ARCH__TRAP_H */
