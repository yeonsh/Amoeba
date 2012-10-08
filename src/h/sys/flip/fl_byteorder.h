/*	@(#)fl_byteorder.h	1.4	94/04/06 17:19:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifdef __BIG_ENDIAN_H__
#define fl_orderhdr(fh) \
    do { \
	if(!(fh->fh_flags & FL_F_BIGENDIAN)) { \
	    dec_s_le(&fh->fh_act_hop);	\
	    dec_s_le(&fh->fh_max_hop); \
	    dec_l_le(&fh->fh_messid); \
	    dec_l_le(&fh->fh_length); \
	    dec_l_le(&fh->fh_total); \
	    dec_l_le(&fh->fh_offset); \
	    if(fh->fh_type == FL_T_HEREIS && fh->fh_length ==\
	       FLIP_HEREIS_LENGTH){\
                 dec_s_le((uint16 *) (((char *) fh) + FLIP_HDR_SIZE)); \
                 dec_s_le((uint16 *) (((char *) fh) + FLIP_HDR_SIZE + sizeof(short))); \
	    } \
	    fh->fh_flags |= FL_F_BIGENDIAN; \
	} \
     } while(0)
#else
#define fl_orderhdr(fh) \
    do { \
	if(fh->fh_flags & FL_F_BIGENDIAN) { \
	    dec_s_be(&fh->fh_act_hop); \
	    dec_s_be(&fh->fh_max_hop); \
	    dec_l_be(&fh->fh_messid); \
	    dec_l_be(&fh->fh_length); \
	    dec_l_be(&fh->fh_total); \
	    dec_l_be(&fh->fh_offset); \
	    if(fh->fh_type == FL_T_HEREIS && fh->fh_length ==\
	       FLIP_HEREIS_LENGTH){\
                 dec_s_be((uint16 *) (((char *) fh) + FLIP_HDR_SIZE)); \
                 dec_s_be((uint16 *) (((char *) fh) + FLIP_HDR_SIZE + sizeof(short))); \
	    } \
	    fh->fh_flags &= ~FL_F_BIGENDIAN; \
	} \
     } while(0)
#endif

#ifdef __BIG_ENDIAN_H__
#define f_setendian(fh)    fh->fh_flags |= FL_F_BIGENDIAN
#else
#define f_setendian(fh)
#endif

