/*	@(#)flip.h	1.3	96/02/27 10:35:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __PROTOCOLS_FLIP_H__
#define __PROTOCOLS_FLIP_H__

/* In this file FLIP addresses and FLIP headers are defined.
 */

#define FLIP_LOC_SIZE	8		/* length of network location */

struct location_bytes {	/* the location on a network segnent */
	char fl_bytes[FLIP_LOC_SIZE];	/* enough bytes to describe that */
};

#define FLIP_ADR_SIZE		8	/* length of FLIP address */

struct flip_address {	/* FLIP address */
	char	fa_abytes[FLIP_ADR_SIZE - 1];
	char	fa_space;		/* address space */
};

/* An address is realy an int64, but most architecture do not support them yet.
 * Thus, we treat an address as two longs, so that we can do fast comparisons
 * and to make sure that they are correctly aligned.
 */
typedef union int64 {
        struct location_bytes	u_loc;
	struct flip_address 	u_addr;
	long 			u_long[2];
} adr_t, *adr_p, location;
	
#define a_abytes	u_addr.fa_abytes
#define a_space		u_addr.fa_space
#define l_bytes		u_loc.fl_bytes

#define ADR_NULL(a)	((a)->u_long[0] == 0L && (a)->u_long[1] == 0L)
#define LOC_NULL(a)	((a)->u_long[0] == 0L && (a)->u_long[1] == 0L)

#define ADR_EQUAL(a1, a2) ((a1)->u_long[0] == (a2)->u_long[0] && \
			   (a1)->u_long[1] == (a2)->u_long[1])
#define LOC_EQUAL(a1, a2) ((a1)->u_long[0] == (a2)->u_long[0] && \
			   (a1)->u_long[1] == (a2)->u_long[1]) 

typedef uint8 	f_vers_t;
typedef uint8 	f_type_t;
typedef uint8 	f_flag_t;
typedef uint8 	f_rsrv_t;
typedef uint16	f_hopcnt_t;
typedef uint32  f_msgcnt_t;
typedef uint32  f_size_t;

/* Flags: */

/* input flags */
#define FLIP_INVAL      0x1     /* invalidate flip address cache */
#define FLIP_SYNC       0x2     /* synchronous message passing */
#define FLIP_SECURITY   0x4     /* use only trusted networks */
#define FLIP_SKIP_SRC	0x8     /* don't deliver multicast message at src */

/* output flags */
#define FLIP_NOTHERE    0x10    /* message came back as a nothere message */
#define FLIP_UNTRUSTED  0x20    /* message came back as a untrusted message */


/* Error values: */
#define FLIP_OK         (0)
#define FLIP_FAIL       (-1)
#define FLIP_TIMEOUT    (-2)
#define FLIP_NOPACKET   (-3)
#define FLIP_UNKNOWN    (-4)
#define FLIP_UNSAFE     (-5)

#endif /* __PROTOCOLS_FLIP_H__ */
