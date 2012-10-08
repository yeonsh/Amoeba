/*	@(#)littl_endian.h	1.3	94/04/06 15:48:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __LITTLE_ENDIAN_H__
#define __LITTLE_ENDIAN_H__

/*
 * set of macros to do inplace byteorder changes
 * The dec_* routines decode a short (_s) or long (_l) from little endian(_le)
 * or bigendian(_be) to native format.
 * The enc_* are similar for native to net format
 *
 * WARNING: -	there are links to this file so never throw it away.  Always
 *		edit it in place.
 */


/* Little-endian version */

#define dec_s_be(s)	(*(s))=(((*(s)>>8)&0xFF)|((*(s)&0xFF)<<8))
#define dec_s_le(s)	/* nothing */

#define dec_l_be(l)	(*(l))=(((*(l)>>24)&0xFF)|((*(l)>>8)&0xFF00)|((*(l)<<8)&0xFF0000)|((*(l)<<24)&0xFF000000))
#define dec_l_le(l)	/* nothing */

#define enc_s_be(s)	dec_s_be(s)
#define enc_s_le(s)	/* nothing */

#define enc_l_be(l)	dec_l_be(l)
#define enc_l_le(l)	/* nothing */

/*
** The LITTLE_ENDIAN definition is to get lance ethernet address
** initialization to work.
*/

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN
#endif

#ifdef BIG_ENDIAN
#include "Both BIG_ENDIAN and LITTLE_ENDIAN are defined"
#endif

#endif /* __LITTLE_ENDIAN_H__ */
