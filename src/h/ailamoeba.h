/*	@(#)ailamoeba.h	1.4	94/04/06 15:37:55 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __AILAMOEBA_H__
#define __AILAMOEBA_H__

#include <amoeba.h>

/*
 *	Ail defines "ail" as 1, which cc isn't supposed to do.
 */

#if ail

marshal capability with in cm_cap, su_cap out cu_cap, sm_cap;
marshal private with in cm_prv, su_prv out cu_prv, sm_prv;

#else /* ail */

/*
 *      Marshal macro's; ail needn't know about these
 */

#define unmsh_t(type, p, arg)	((*(arg) = *(type *) p), p + sizeof(type))
#define msh_t(type, p, arg)	(*((type *)p) = (arg), p + sizeof(type))

#define cm_prv(p, prv)	msh_t(private, p, prv)
#define cu_prv(p, prv)	unmsh_t(private, p, prv)
#define sm_prv(p, prv)	msh_t(private, p, prv)
#define su_prv(p, prv)	unmsh_t(private, p, prv)

#define cm_cap(p, cap)	msh_t(capability, p, cap)
#define cu_cap(p, cap)	unmsh_t(capability, p, cap)
#define sm_cap(p, cap)	msh_t(capability, p, cap)
#define su_cap(p, cap)	unmsh_t(capability, p, cap)

/*
 *	Ail standard errors
 *	Analogous to <stderr.h> for ail-products. Currently maps ail-
 *	errors on old std-errors. We should either change ail to return
 *	real std-errors, or claim a separate range of error codes for
 *	ail-generated RPC-interfaces. Some day.
 */
#include <cmdreg.h>
#include <stderr.h>

/*
 *	For ail:
 *	Name ail uses:	Std-error:	Where:	When:
 */
#define AIL_CLN_IN_BAD	STD_ARGBAD	/* Client marshal	*/
#define AIL_SVR_IN_BAD	STD_ARGBAD	/* Server unmarshal	*/
#define AIL_SVR_OUT_BAD	STD_ARGBAD	/* Server marshal	*/
#define AIL_CLN_OUT_BAD	STD_COMBAD	/* Client unmarshal	*/
#define AIL_H_COM_BAD	STD_COMBAD	/* Server switch(h_com)	*/
#define AIL_CLN_MEM_BAD	STD_NOMEM	/* Client malloc buffer	*/
#define AIL_SVR_MEM_BAD	STD_NOMEM	/* Server malloc buffer	*/


/*
 *	An ailword is used to describe the machine
 *	representations of integers (which are required
 *	to take four bytes in a message - no precision guarantueed)
 *	and in the future probably also floats and the like.
 *
 *	For efficiency, we attempt to make it a constant:
 */

#ifdef lint
/* avoid redeclaration of _AIL_ENDIAN */
#       define _AIL_ENDIAN _AIL_B_ENDIAN
#else  /* !lint */

#ifdef tahoe		/* Harris' HCX-7 is a huge big endian	*/
#	define _AIL_ENDIAN _AIL_B_ENDIAN
#endif /* tahoe */

#if defined(vax) || defined(__vax__)	    /* The vax is a little endian */
#	define _AIL_ENDIAN _AIL_L_ENDIAN
#endif /* vax */

#if defined(i80386) || defined(__i80386__)  /* intel 80386 */
#	define _AIL_ENDIAN _AIL_L_ENDIAN
#endif /* i80386 */

#if defined(mc68000) || defined(__mc68000__) /* mc680X0s are big endians */
#	define _AIL_ENDIAN _AIL_B_ENDIAN
#endif /* mc68000 */

#if defined(sparc) || defined(__sparc__)
#	define _AIL_ENDIAN _AIL_B_ENDIAN
#endif /* sparc */

#ifdef MIPSEB		/* BIG ENDIAN R2000 */
#       define _AIL_ENDIAN _AIL_B_ENDIAN
#endif
#ifdef MIPSEL		/* little endian r2000 */
#       define _AIL_ENDIAN _AIL_L_ENDIAN
#endif

#ifdef _any_hardware	/* Any hardware is an omni endian	*/
#	define _AIL_ENDIAN _AIL_B_ENDIAN
#	define _AIL_ENDIAN _AIL_L_ENDIAN
#endif /* _any_hardware */

#endif /* lint */

/************************************************************************\
 *									*
 *	See if we can come up with a constant expression:		*
 *									*
\************************************************************************/

#ifdef _AIL_ENDIAN
	/*
	 *	Joepie! We can put a constant expression here:
	 */
#	define _ailword	_AIL_ENDIAN
#else
	/*
	 *	Let's hope the library knows the _ailword:
	 */
	extern short _ailword;
#endif /* _AIL_ENDIAN */

/************************************************************************\
 *									*
 *	Define the constants that make up any of the above:		*
 *									*
\************************************************************************/

/*
 *	Ints:
 */
#define AIL_INTMASK	0x00000007
#define _AIL_B_ENDIAN	0x00000001
#define _AIL_L_ENDIAN	0x00000002

/*
 *	Floats (not used currently, but to get the intention):
 */
#define AIL_FLTMASK	0x0000000d

#endif /* ail */

#endif /* __AILAMOEBA_H__ */
