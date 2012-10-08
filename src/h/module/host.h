/*	@(#)host.h	1.4	96/02/27 10:32:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
host.h

Created Nov 6, 1991 by Philip Homburg


declaration of host access routines
*/

#ifndef __MODULE__HOST_H__
#define __MODULE__HOST_H__

#define	host_lookup		_host_lookup
#define	super_host_lookup	_super_host_lookup
#define	ip_host_lookup		_ip_host_lookup
#define	syscap_lookup		_syscap_lookup

errstat host_lookup _ARGS(( char *host_name, capability *cap ));
errstat super_host_lookup _ARGS(( char *host_name, capability *cap ));
errstat ip_host_lookup _ARGS(( char *host_name, char *ext, capability *cap ));
errstat syscap_lookup _ARGS(( char *host, capability *cap ));


#endif /* __MODULE__HOST_H__ */
