/*	@(#)ioctl.h	1.3	96/02/27 14:14:49 */
/*
ioctl.h

Copyright 1994 Philip Homburg
*/

/* The ioctl.h header is used for controlling network lines */

#ifndef _IOCTL_H
#define _IOCTL_H

/* ioctl's have the command encoded in the low-order word, and the size
 * of the parameter in the high-order word. The 2 high bits of the high-
 * order word are used to encode the in/out status of the parameter.
*/

#define IOCPARM_MASK	0x3FFFL
#define IOCTYPE_MASK	0xFFFFL
#define IOC_IN		0x40000000L
#define IOC_OUT		0x80000000L
#define IOC_INOUT	(IOC_IN | IOC_OUT)

#define _IO(x,y)	((x << 8) | y)
#define _IOR(x,y,t)	((x << 8) | y  | ((sizeof(t) & IOCPARM_MASK) << 16) |\
				IOC_IN)
#define _IOW(x,y,t)	((x << 8) | y | ((sizeof(t) & IOCPARM_MASK) << 16) |\
				IOC_OUT)
#define _IORW(x,y,t)	((x << 8) | y | ((sizeof(t) & IOCPARM_MASK) << 16) |\
				IOC_INOUT)


#define NWIOSETHOPT	_IOR('n',16,struct nwio_ethopt)
#define NWIOGETHOPT	_IOW('n',17,struct nwio_ethopt)
#define NWIOGETHSTAT	_IOW('n',18,struct nwio_ethstat)

#define NWIOSIPCONF	_IOR('n',32,struct nwio_ipconf)
#define NWIOGIPCONF	_IOW('n',33,struct nwio_ipconf)
#define NWIOSIPOPT	_IOR('n',34,struct nwio_ipopt)
#define NWIOGIPOPT	_IOW('n',35,struct nwio_ipopt)

#define NWIOGIPOROUTE   _IORW('n', 40, struct nwio_route)
#define NWIOSIPOROUTE   _IOW ('n', 41, struct nwio_route)
#define NWIODIPOROUTE   _IOW ('n', 42, struct nwio_route)
#define NWIOGIPIROUTE   _IORW('n', 43, struct nwio_route)
#define NWIOSIPIROUTE   _IOW ('n', 44, struct nwio_route)
#define NWIODIPIROUTE   _IOW ('n', 45, struct nwio_route)

#define NWIOSTCPCONF	_IOR('n',48,struct nwio_tcpconf)
#define NWIOGTCPCONF	_IOW('n',49,struct nwio_tcpconf)
#define NWIOTCPCONN	_IOR('n',50,struct nwio_tcpcl)
#define NWIOTCPLISTEN	_IOR('n',51,struct nwio_tcpcl)
#define NWIOTCPATTACH	_IOR('n',52,struct nwio_tcpatt)
#define NWIOTCPSHUTDOWN	_IO('n', 53)
#define NWIOSTCPOPT	_IOR('n',54,struct nwio_tcpopt)
#define NWIOGTCPOPT	_IOW('n',55,struct nwio_tcpopt)

#define NWIOSUDPOPT	_IOR('n',64,struct nwio_udpopt)
#define NWIOGUDPOPT	_IOW('n',65,struct nwio_udpopt)

#endif /* _IOCTL_H */
