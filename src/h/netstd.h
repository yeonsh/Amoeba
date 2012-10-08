/*	@(#)netstd.h	1.2	94/04/06 15:42:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __NETSTD_H__
#define __NETSTD_H__

#define OBJSIZE		3

#define nettochr(p, cp)		(*(cp) = *(p))
#define chrtonet(cp, p)		(*(p) = *(cp))
#define nettouns(p, up)		(*(up) = (p)[0] << 8 | (p)[1] & 0xFF)
#define nettosrt(p, sp)		(*(sp) = (p)[0] << 8 | (p)[1] & 0xFF)
#define srttonet(sp, p)		unstonet((uint16 *) (sp), p)
#define nettouni(p, ip)		(*(ip) = (p)[0] << 8 | (p)[1] & 0xFF)
#define nettoint(p, ip)		(*(ip) = (short) ((p)[0] << 8 | (p)[1] & 0xFF))
#define inttonet(ip, p)		unitonet((unsigned *) (ip), p)
#define Lshift(p, i, s)		(((long)(p)[i] & 0xFF) << s)
#define nettolng(p, lp)		(*(lp) = Lshift(p, 0, 24) | Lshift(p, 1, 16) \
					| Lshift(p, 2, 8) | Lshift(p, 3, 0))
#define nettoobj(p, q)		netcopy(p, q, OBJSIZE)
#define objtonet(q, p)		netcopy(q, p, OBJSIZE)
#define nettoprt(p, q)		netcopy(p, (char *) (q), PORTSIZE)
#define prttonet(q, p)		netcopy((char *) (q), p, PORTSIZE)

#endif /* __NETSTD_H__ */
