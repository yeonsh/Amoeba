/*	@(#)types.h	1.2	94/04/06 11:35:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Boolean definitions
 */
#define	TRUE	1
#define	FALSE	0

/*
 * Useful short hands
 */
typedef char int8;
typedef short int int16;
typedef long int int32;

typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef unsigned long int uint32;

/*
 * Little endian byte order reversal macros
 * Beware for side effects !
 */
#define htons(x)	(((((uint16)(x))>>8)&0xff)|((((uint16)(x))&0xff)<<8))
#define ntohs(x)	(((((uint16)(x))>>8)&0xff)|((((uint16)(x))&0xff)<<8))

#define htonl(x)	((((x)>>24)&0xffL)|(((x)>>8)&0xff00L) | \
			(((x)<<8)&0xff0000L)|(((x)<<24)&0xff000000L))
#define ntohl(x)	((((x)>>24) & 0xffL)|(((x)>>8)&0xff00L) | \
			(((x)<<8)&0xff0000L)|(((x)<<24)&0xff000000L))

