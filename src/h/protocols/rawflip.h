/*	@(#)rawflip.h	1.3	96/02/27 10:35:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __PROTOCOLS_RAWFLIP_H__
#define __PROTOCOLS_RAWFLIP_H__

struct rawflip_upcall {
	adr_t		rfu_src;
	adr_t		rfu_dst;
	long		rfu_messid;
	long		rfu_offset;
	f_size_t	rfu_len;
	long		rfu_tlen;
	int		rfu_flags;
	int		rfu_result;
};

struct rawflip_parms {
	int		rf_opcode;
	int		rf_if;
	int		rf_ep;
	char *		rf_pkt;
	f_hopcnt_t	rf_hop;
	interval	rf_ltime;
	int		rf_n;
	struct rawflip_upcall rf_u;
};

#define rf_src		rf_u.rfu_src
#define rf_dst		rf_u.rfu_dst
#define rf_messid	rf_u.rfu_messid
#define rf_offset	rf_u.rfu_offset
#define rf_len		rf_u.rfu_len
#define rf_tlen		rf_u.rfu_tlen
#define rf_flags	rf_u.rfu_flags
#define rf_result	rf_u.rfu_result

#define RF_OPC_UPCALL		0
#define RF_OPC_INIT		1
#define RF_OPC_END		2
#define RF_OPC_REGISTER		3
#define RF_OPC_UNREGISTER	4
#define RF_OPC_BROADCAST	5
#define RF_OPC_MULTICAST	6
#define RF_OPC_UNICAST		7

#define UPCALL_RESULT_RECEIVED	1
#define UPCALL_RESULT_NDLIVERED	2
#define UPCALL_RESULT_EXIT	3
#define UPCALL_RESULT_ABORTED	4

#endif /* __PROTOCOLS_RAWFLIP_H__ */
