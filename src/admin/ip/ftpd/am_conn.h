/*	@(#)am_conn.h	1.1	96/02/27 10:13:46 */
#ifndef AM_CONN_H
#define AM_CONN_H

#include <amoeba.h>

#define CONN_ALLOC	0x01
#define CONN_OPEN	0x02
#define CONN_CLOSED	0x04
#define CONN_EOF	0x08
#define CONN_ERR	0x10

/* Status kept for each Amoeba TCP connection: */
typedef struct {
    int        tcp_nr;	    /* connection number */
    int        tcp_status;  /* open, last read or written? */
    capability tcp_cap;     /* tcp channel capability */
    char      *tcp_buf;     /* for write buffering */
    int        tcp_bufsiz;  /* size of tcp_buf */
    int        tcp_inbuf;   /* # bytes in tcp_buf */
    struct circbuf
	      *tcp_circbuf; /* input made available by reader thread */
} tcpconn;

#include <server/ip/types.h>
#include <server/ip/tcpip.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/inet.h>
#include <server/ip/gen/tcp.h>

extern ipaddr_t  data_dest_addr;
extern tcpport_t data_dest_port;

#endif
