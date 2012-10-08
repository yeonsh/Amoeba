/*	@(#)internet.h	1.3	94/04/06 17:18:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __INTERNET_H__
#define __INTERNET_H__

#define PACKETSIZE	1490	/* network packet size - sizeof(framehdr) */

#define BROADCAST	((address) 0xFF)

#define TYPE		0x0F	/* message types */
#define   LOCATE	0x01
#define   HERE		0x02
#define   REQUEST	0x03
#define   REPLY		0x04
#define   ACK		0x05
#define   NAK		0x06
#define	  ENQUIRY	0x07
#define   ALIVE		0x08
#define	  DEAD		0x09

#define LAST		0x10	/* flags */
#define RETRANS		0x20

struct pktheader {
	char	ph_dstnode;	/* 0: destination node */
	char	ph_srcnode;	/* 1: source node */
	char	ph_dstthread;	/* 2: destination thread */
	char	ph_srcthread;	/* 3: source thread */
	char	ph_ident;	/* 4: transaction identifier */
	char	ph_seq;		/* 5: fragment no. */
	uint16	ph_size;	/* 6: total size of this packet */
	char	ph_flags;	/* 8: some flags (not used) */
	char	ph_type;	/* 9: locate, here, data, ack or nak (!= 0) */
};

#define ph_signal	ph_seq

#define NOSEND		0
#define SEND		1

#define DONTKNOW	0
#define LOCAL		1
#define GLOBAL		2

#define siteaddr(x)	lobyte(x)
#define threadnum(x)	hibyte(x)

#define pktfrom(ph)	((uint16) (ph->ph_srcthread<<8 | ph->ph_srcnode & 0xFF))
#define pktto(ph)	((uint16) (ph->ph_dstthread<<8 | ph->ph_dstnode & 0xFF))

#endif /* __INTERNET_H__ */
