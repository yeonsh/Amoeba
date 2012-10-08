/*	@(#)rpc_endian.h	1.2	94/04/06 08:46:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifdef __BIG_ENDIAN_H__
#define rpc_orderhdr(ah, bigendian) \
    do { \
	if(!bigendian) { \
	    dec_l_le(&(ah)->ah_tid); \
	    dec_s_le(&(ah)->ah_dest); \
	    dec_s_le(&(ah)->ah_from); \
	} \
     } while(0)
#else
#define rpc_orderhdr(ah, bigendian) \
    do { \
	if(bigendian) { \
	    dec_l_be(&(ah)->ah_tid); \
	    dec_s_be(&(ah)->ah_dest); \
	    dec_s_be(&(ah)->ah_from); \
	} \
     } while(0)
#endif


#ifdef __BIG_ENDIAN_H__
#define rpc_setendian(ah)    (ah)->ah_flags |= FL_BIGENDIAN
#else
#define rpc_setendian(ah)
#endif
