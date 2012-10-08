/*	@(#)tftp.h	1.2	94/04/06 11:35:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Trivial File Transfer Protocol (see RFC 783)
 */

#define	SEGSIZE		512		/* data segment size */

/*
 * Packet types
 */
#define	TFTP_RRQ	01		/* read request */
#define	TFTP_WRQ	02		/* write request */
#define	TFTP_DATA	03		/* data packet */
#define	TFTP_ACK	04		/* acknowledgement */
#define	TFTP_ERROR	05		/* error code */

/*
 * TFTP header structure
 */
typedef struct {
    int16	th_op;			/* packet type */
    union {
	int16	tu_block;		/* block # */
	int16	tu_code;		/* error code */
	char	tu_stuff[1];		/* request packet stuff */
    } th_u;
} tftphdr_t;

/* for ease of reference */
#define	th_block	th_u.tu_block
#define	th_code		th_u.tu_code
#define	th_stuff	th_u.tu_stuff
#define	th_data		th_stuff[2]
#define	th_msg		th_data

