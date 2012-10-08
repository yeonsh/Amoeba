/*	@(#)tcpip.cls	1.2	94/04/06 15:54:18 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
**
** Amoeba IP, version 4.
**
*/

#include <amoeba.h>
#include <std.cls>


class tcpip [28300 .. 28399] {
	inherit stdinfo, stdstatus, stdrestrict;
	
	/* Rights bits */
	const TCPR_LSN	= 2;		/* (server) Listen right */
	const TCPR_CON	= 4;		/* (server) Connect right */
	const TCPR_PCON = 8;		/* (server) priv-conn right */


	typedef unsigned char host_t[4];


	/*
	** tcpip_listen - Listen to a port.
	** Listen waits for a connect request to come in on the specified
	** port, sets up a virtual circuit and returns the port
	** for the VC.
	*/
	tcpip_listen(*,
	    in int poort,		/* port number */
	    out port res1,		/* Half of the result */
	    out port res2);		/* Other half */

	/*
	** tcpip_connect - Connect to a server.
	** Connect tries to connect to the specified port on the specified
	** machine. A VC is setup and the port for the VC returned.
	*/
	tcpip_connect(*,
	    in host_t host,		/* Host to connect to */
	    in int poort,		/* Port to connect to */
	    out port res1,		/* the */
	    out port res2);		/* Resultant vc */
	/*
	** tcpip_pconnect - Connect to a server.
	** Same as tcpip_connect, but uses a privileged IP port.
	*/

	tcpip_pconnect(*,
	    in host_t host,		/* Host to connect to */
	    in int poort,		/* Port to connect to */
	    out port res1,		/* the */
	    out port res2);		/* Resultant vc */


	/*
	** tcpip_stat - Connection status.
	** Calling this routine causes the tcp/ip server to print
	** a dump of its connections.
	**
	** The '1024' is an artefact of the current ail implementation.
	** It will also be 'len' eventually.
	
	tcpip_stat(*, out char buf[len:1024], in out int len);
	*/

	/*
	** tcpip_lookup - Map hostname to hostid.
	** This is not actually a server stub.
	*/
	tcpip_lookup(in char hostname[], out host_t hostid);

	/*
	** tcpip_listennew - Listen to a port.
	** Listen waits for a connect request to come in on the specified
	** port, sets up a virtual circuit and returns the port
	** for the VC.
	*/
	tcpip_newlisten(*,
	    in int poort,		/* port number */
	    out port res1,		/* Half of the result */
	    out port res2,		/* Other half */
	    in out char source[20],
	    in out char dest[20]);

};
