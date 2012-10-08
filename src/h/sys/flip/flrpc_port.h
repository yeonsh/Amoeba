/*	@(#)flrpc_port.h	1.1	96/02/27 10:39:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FLRPC_PORT_H__
#define __FLRPC_PORT_H__

/* Per server thread there is a port cache entry. */
typedef struct flportcache {
    port	p_pubport;	/* public port of the server */
    adr_t 	p_addr;		/* flip address for this port. */
#ifdef SPEED_HACK1
    location	p_loc;
    int		p_ntw;
    f_hopcnt_t	p_hopcnt;
#endif
    interval	p_roundtrip;	/* estimated time for round trip to server */
    uint16	p_thread;	/* for a local server a thread index */
    int		p_idle;		/* idle time */
    int 	p_where;	/* local or remote */
    char	*p_arg;		/* to additional arg for optimizations */
    struct flportcache *p_next;
} portcache_t, *portcache_p;


void port_init _ARGS(( void ));
void port_install _ARGS(( uint16 thread, adr_p a, port *p, int where,
					    interval roundtrip, char *arg ));
void port_remove _ARGS(( port *prt, adr_p adr, uint16 thread, int owner ));
portcache_p port_lookup _ARGS(( port * p ));

#define P_DELETE	1
#define P_NODELETE	2

#define P_LOCAL		3
#define P_SOMEWHERE	4

#endif /* __FLRPC_PORT_H__ */
