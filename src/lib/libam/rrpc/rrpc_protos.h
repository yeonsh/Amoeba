/*	@(#)rrpc_protos.h	1.2	94/04/07 10:18:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __RRPC_PROTOS_H__
#define	__RRPC_PROTOS_H__

void msg_grp_forward _ARGS(( GROUP *gp, message *mp ));
void paddr _ARGS(( address * a ));
void _delete_context_ref _ARGS(( CTXT_NODE * cp ));
void _replicated_context_remove _ARGS(( message * msg ));

#endif /* __RRPC_PROTOS_H__ */
