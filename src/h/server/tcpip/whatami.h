/*	@(#)whatami.h	1.2	94/04/06 17:16:24 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __WHATAMI_H__
#define __WHATAMI_H__

/*
** whatami.h - Machine definitions.
*/

#include "byteorder.h"

/*
*   Defines which have to do with Ethernet addressing versus Appletalk
*   addressing.  Ethernet has 6 bytes of hardware address, ATALK has 4
*/
#define DADDLEN 6
#define WINDOWSIZE 4096
#define TSENDSIZE 512
#define DEFWINDOW 1024
#define DEFSEG	1024
#define TMAXSIZE 1024
#define UMAXLEN 1024
#define ICMPMAX 300 

typedef unsigned int	uint;

#define NPORTS		30
#define CONNWAITTIME	20

#define movebytes(to,from,n) (void) memcpy((_VOIDSTAR)(to), (_VOIDSTAR)(from), (n))
#define movenbytes(to,from,n) (void) memcpy((_VOIDSTAR)(to), (_VOIDSTAR)(from), (n))

#ifdef LITTLE_ENDIAN
short intswap();
long longswap();
#else
#define longswap(x) (x)
#define intswap(x)  (x)
#endif

#endif /* __WHATAMI_H__ */
