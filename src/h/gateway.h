/*	@(#)gateway.h	1.2	94/04/06 15:40:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __GATEWAY_H__
#define __GATEWAY_H__

#define BUFFERSIZE	2048

#define LOGDFLT		8	/* Default logging level */

struct framehdr {
	char fh_type;		/* REQUEST, REPLY, or GREETINGS */
	char fh_mpx;		/* client */
	char fh_reserved[2];
};

struct frame {		/* frames as handled by gateway switch */
	struct framehdr f_frhd;	/* frame header */
	header f_hdr;		/* transaction header */
	char f_buf[BUFFERSIZE];	/* transaction buffer */
};

#define f_type	f_frhd.fh_type
#define f_mpx	f_frhd.fh_mpx

struct bufhdr {		/* used for pipe communication */
	char bh_type;		/* OPEN, DATA, or CLOSE */
	char bh_reserved;
	char bh_hilength;	/* actual length of b_u */
	char bh_lolength;	/* ditto */
};

struct buffer {		/* this is sent on pipes */
	struct bufhdr b_hdr;
	union {
		char bu_data[sizeof(struct frame) + 1];
		struct frame bu_frame;
	} b_u;
};

#define b_type		b_hdr.bh_type
#define b_length(b)	((b).b_hdr.bh_hilength<<8 | (b).b_hdr.bh_lolength&0xFF)
#define b_data		b_u.bu_data
#define b_frame		b_u.bu_frame

/* bh_type or b_type */
#define OPEN		 1	/* open a connection */
#define DATA		 2	/* send data over connection */
#define CLOSE		 3	/* close connection */
char *str_btype();

/* f_type */
#define REQUEST		 1	/* request message */
#define REPLY		 2	/* reply message */
#define GREETINGS	 3	/* greetings message */

/* h_command */
#define SEND		 1	/* data arrived */
#define DONE		 2	/* open, data, or close request done */
#define FAILED		 3	/* connection failure */
#define INSTALL		 4	/* install a port */
#define REMCONN		 5	/* remote gateway initiates connection */
#define CONNECT		 6	/* add a wan process */
#define SWEEP		 7	/* timer sweep */
#define SHUTDOWN	 8	/* shut gateway down */
#define REFUSED		 9	/* refused CLOSE request */
#define DELETE		10	/* delete port */
#define DUMP		11	/* dump internal tables */
#define DB_ADD		12	/* add port to database */
#define DB_DEL		13	/* delete port from database */
char *str_cmd();

/* h_status */
#define FOUND_IT	 0	/* command succeeded */
#define NOT_FOUND	-1	/* command failed */
#define TRY_AGAIN	-2	/* out of resources */

/* initialization data size */
#define INITSIZE	(PORTSIZE + sizeof(capability))

#endif /* __GATEWAY_H__ */
