/*	@(#)am_endian.h	1.2	94/04/06 08:44:27 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifdef __BIG_ENDIAN_H__
#define am_orderhdr(hdr, bigendian) \
    do { \
	if(!bigendian) { \
	    dec_l_le(&(hdr)->h_offset); \
	    dec_s_le(&(hdr)->h_extra); \
	    dec_s_le(&(hdr)->h_command); \
	    dec_s_le(&(hdr)->h_size); \
	} \
     } while(0)
#else
#define am_orderhdr(hdr, bigendian) \
    do { \
	if(bigendian) { \
	    dec_l_be(&(hdr)->h_offset); \
	    dec_s_be(&(hdr)->h_extra); \
	    dec_s_be(&(hdr)->h_command); \
	    dec_s_be(&(hdr)->h_size); \
	} \
     } while(0)
#endif

