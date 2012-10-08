/*	@(#)Mund.S	1.3	96/02/27 11:10:34 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Mund.S
 *
 *	Convert double to int - this is really only here because the old
 *	sun3 C compiler generates calls to it.
 *
 * Author:	Gregory J. Sharp, Feb 1994
 */

#include "assyntax.h"

#define	CONST1	0x4F000000
#define	CONST2	0x4E100000

AS_BEGIN

GLOBLDIR Mund
Mund:
#if !defined(ACK_ASSEMBLER) && !defined(ACE_ASSEMBLER) && !defined(GNU_ASSEMBLER)
	MOVEML	d0-d1,AUTODEC(sp)	COM push argument onto stack
	CMPL	#CONST2, d0		COM where we can get at it
	JLT	1f
	fmoved	AUTOINC(sp), fp0
	fsubs	#CONST1, fp0
	fintrzx	fp0, fp0
	fmovel	fp0, d0
	bchg	#0x1f, d0
	rts
1:	
	fintrzd	AUTOINC(sp), fp0
	fmovel	fp0, d0
#endif
	rts
