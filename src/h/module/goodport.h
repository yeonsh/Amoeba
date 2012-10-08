/*	@(#)goodport.h	1.3	96/02/27 10:32:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_GOODPORT_H__
#define __MODULE_GOODPORT_H__
/*
 * This contains the declarations for the "goodport" module, used for avoiding
 * timeout errors when doing repeated transactions to bad ports.  Bad ports
 * are defined as ones that have previously given RPC_NOTFOUND errors when
 * a transaction to them was attempted, indicating that a server is down.
 *
 * It provides alternative versions of many standard transaction functions,
 * using "gp_" as a prefix to the standard name.  The only difference in
 * functionality is that a second attempt to do a transaction to a bad port
 * will result in the error RPC_BADPORT, with no attempt to contact the server.
 * (This is intentionally different from RPC_NOTFOUND, so that users can
 * tell the difference, perhaps printing an error message on the first use.)
 * Note that attempting a transaction with a NULL port will also produce the
 * RPC_BADPORT error, but the caller is expected to distinguish this case by
 * noting whether or not the port is NULL.
 */
#include <amoeba.h>
#include <module/stdcmd.h>

#define GP_LOOKUP 0	/* Ask if a port is in the list of bad ports. */
#define GP_APPEND 1	/* Add a port to the list. */
#define GP_DELETE 2	/* Delete a port from the list. */
#define GP_INIT   3	/* Reset the list of bad ports to empty. */

#define	gp_badport	_gp_badport
#define	gp_notebad	_gp_notebad

#define gp_trans(cap, trans_func_call) ( \
		(!gp_badport(&(cap)->cap_port, GP_LOOKUP)) ? \
			gp_notebad(cap, trans_func_call) \
		: \
			RPC_BADPORT \
	)

#define gp_std_touch(cap) \
	        gp_trans(cap, std_touch(cap))
#define gp_std_info(cap, buf, n, len) \
	        gp_trans(cap, std_info(cap, buf, n, len))
#define gp_std_status(cap, buf, n, len) \
	        gp_trans(cap, std_status(cap, buf, n, len))
#define gp_std_copy(server, sourcecap, newcap) \
	        gp_trans(server, std_copy(server, sourcecap, newcap))
#define gp_std_destroy(cap) \
	        gp_trans(cap, std_destroy(cap))
#define gp_std_restrict(cap, mask, new) \
	        gp_trans(cap, std_restrict(cap, mask, new))

int     gp_badport _ARGS(( port *, int ));
errstat gp_notebad _ARGS(( capability *, errstat ));

#endif /* __MODULE_GOODPORT_H__ */
