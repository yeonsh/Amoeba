/*	@(#)tod.h	1.3	94/04/06 17:16:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __TOD_H__
#define __TOD_H__

/* name of time of day server's processor directory entry */
#define	TOD_SVR_NAME	"tod"

#define TOD_GETTIME	(TOD_FIRST_COM)
#define TOD_SETTIME	(TOD_FIRST_COM + 1)

#define RGT_ALL		((rights_bits) 0xff)

#define tod_encode(hdr,sec,msec,tz,dst)				\
		do { 						\
			(hdr)->h_offset = sec;			\
			(hdr)->h_size = (bufsize) msec;		\
			(hdr)->h_extra = dst;			\
		} while(0)

#define tod_decode(hdr,sec,msec,tz,dst)				\
		do {						\
			*sec = (hdr)->h_offset;			\
			*msec = (short) (hdr)->h_size;		\
			*tz = 0;				\
			*dst = (hdr)->h_extra;			\
		} while(0)

#endif /* __TOD_H__ */
