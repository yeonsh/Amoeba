/*	@(#)grp_endian.h	1.5	94/04/06 08:39:48 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __GRP_ENDIAN_H__
#define __GRP_ENDIAN_H__

/* See which byteorder we are by including "byteorder.h" */
#include "byteorder.h"

#ifdef __BIG_ENDIAN_H__
#define BC_BIGENDIAN	  1
#define dec_s_other	  dec_s_le
#define dec_l_other	  dec_l_le
#define other_endian(big) (!(big))
#define bc_setendian(bc)  (bc)->b_flags |= F_BIGENDIAN
#else  /* __BIG_ENDIAN_H__ */
#define BC_BIGENDIAN	  0
#define dec_s_other	  dec_s_be
#define dec_l_other	  dec_l_be
#define other_endian(big) (big)
#define bc_setendian(bc)  (bc)->b_flags &= ~F_BIGENDIAN
#endif /* __BIG_ENDIAN_H__ */


#define bc_orderhdr(bc, bigendian) 		\
	if (other_endian(bigendian)) {		\
	    dec_s_other(&(bc)->b_cpu);		\
	    dec_s_other(&(bc)->b_incarnation);	\
	    dec_l_other(&(bc)->b_seqno);	\
	    dec_l_other(&(bc)->b_messid);	\
	    dec_l_other(&(bc)->b_expect);	\
	    bc_setendian(bc);			\
	} else 0

#define bc_orderhereis(bigendian, mg, nh, lm, r, l, i, s) \
	if (other_endian(bigendian)) {	\
	    dec_s_other(&(mg));		\
	    dec_s_other(&(nh));		\
	    dec_s_other(&(lm));		\
	    dec_s_other(&(r));		\
	    dec_l_other(&(l));		\
	    dec_s_other(&(i));		\
	    dec_s_other(&(s));		\
	} else 0

#define bc_ordermember(bigendian, used, expect, messid) \
	if (other_endian(bigendian)) {	\
	    dec_l_other(&(used));	\
	    dec_l_other(&(expect));	\
	    dec_l_other(&(messid));	\
	} else 0

#define bc_ordergroup(bigendian, total, seqid, nextseqno, incar, resil, large)\
	if (other_endian(bigendian)) {	\
	    dec_s_other(&(total));	\
	    dec_s_other(&(seqid));	\
	    dec_l_other(&(nextseqno));	\
	    dec_s_other(&(incar));	\
	    dec_s_other(&(resil));	\
	    dec_l_other(&(large));	\
	} else 0

#define bc_orderindex(bigendian, index) \
	if (other_endian(bigendian)) {	\
	    dec_s_other(&(index));	\
	} else 0

#define bc_orderint(bigendian, i)	\
	if(other_endian(bigendian)) {	\
	    dec_l_other(&(i));		\
	} else 0

#endif /* __GRP_ENDIAN_H__ */
