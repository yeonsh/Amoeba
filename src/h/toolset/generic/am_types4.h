/*	@(#)am_types4.h	1.2	94/04/06 17:21:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __AM_TYPES4_H__
#define __AM_TYPES4_H__

/*
 * For use in network dependent structures that must be of a fixed length.
 */
typedef	long		int32;
typedef	short		int16;
typedef	char		int8;
typedef	unsigned long	uint32;
typedef	unsigned short	uint16;
typedef	unsigned char	uint8;

/* macros for encoding/decoding to/from little/big endian */
#define	ENC_INT32_LE(x)		enc_l_le(x)
#define	DEC_INT32_LE(x)		dec_l_le(x)
#define	ENC_INT32_BE(x)		enc_l_be(x)
#define	DEC_INT32_BE(x)		dec_l_be(x)
#define	ENC_INT16_LE(x)		enc_s_le(x)
#define	DEC_INT16_LE(x)		dec_s_le(x)
#define	ENC_INT16_BE(x)		enc_s_be(x)
#define	DEC_INT16_BE(x)		dec_s_be(x)

#define buf_put_int8		buf_put_char
#define	buf_put_int16		buf_put_short
#define	buf_put_int32		buf_put_long
#define buf_get_int8		buf_get_char
#define	buf_get_int16		buf_get_short
#define	buf_get_int32		buf_get_long

#endif /* __AM_TYPES4_H__ */
