/*	@(#)int_rawflip.c	1.4	96/02/27 11:01:30 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "protocols/flip.h"
#include "module/rawflip.h"
#include "stderr.h"
#include "thread.h"

static
struct admin {
	int	a_inuse;
	int	a_if;
	long	a_ident;
	void	(*a_rcv)();
	void	(*a_ndl)();
} a;

static void
flip_upcall(param, size)
char *param;
int size;
{
	struct rawflip_parms args;
	static char buf[2000];

	for (;;) {
		args.rf_opcode = RF_OPC_UPCALL;
		args.rf_if = a.a_if;
		args.rf_pkt = buf;
		args.rf_len = sizeof(buf);
		rawflip(&args);
		switch (args.rf_result) {
		case UPCALL_RESULT_EXIT:
			thread_exit();
			break;
		case UPCALL_RESULT_RECEIVED:
			(*a.a_rcv)(a.a_ident, buf, &args.rf_dst, &args.rf_src,
				args.rf_messid, args.rf_offset, args.rf_len,
				args.rf_tlen, args.rf_flags);
			break;
		case UPCALL_RESULT_NDLIVERED:
			(*a.a_ndl)(a.a_ident, buf, &args.rf_dst,
				args.rf_messid, args.rf_offset, args.rf_len,
				args.rf_tlen, args.rf_flags);
			break;
		}
	}
}

flip_init(ident, receive, notdeliver)
long ident;
void (*receive)();
void (*notdeliver)();
{
	struct rawflip_parms args;

	if (a.a_inuse)
		return FLIP_FAIL;
	args.rf_opcode = RF_OPC_INIT;
	rawflip(&args);
	a.a_inuse = 1;
	a.a_if = args.rf_if;
	a.a_ident = ident;
	a.a_rcv = receive;
	a.a_ndl = notdeliver;
	if (!thread_newthread(flip_upcall, 65536, (char *) 0, 0)) {
		printf("thread_newthread failed\n");
	}
	return args.rf_if;
}

flip_end(ifno)
int ifno;
{
	struct rawflip_parms args;

	args.rf_opcode = RF_OPC_END;
	args.rf_if = ifno;
	rawflip(&args);
	a.a_inuse = 0;
	return args.rf_result;
}

flip_register(ifno, adr)
int ifno;
adr_p adr;
{
	struct rawflip_parms args;

	args.rf_opcode = RF_OPC_REGISTER;
	args.rf_if = ifno;
	args.rf_src = *adr;
	rawflip(&args);
	return args.rf_ep;
}

flip_unregister(ifno, ep)
int ifno, ep;
{
	struct rawflip_parms args;

	args.rf_opcode = RF_OPC_UNREGISTER;
	args.rf_if = ifno;
	args.rf_ep = ep;
	rawflip(&args);
	return args.rf_result;
}

#ifdef __STDC__
flip_broadcast( int ifno, char *pkt, int ep, f_size_t length, f_hopcnt_t hopcnt)
#else
flip_broadcast(ifno, pkt, ep, length, hopcnt)
int ifno;
char *pkt;
int ep;
f_size_t length;
f_hopcnt_t hopcnt;
#endif
{
	struct rawflip_parms args;

	args.rf_opcode = RF_OPC_BROADCAST;
	args.rf_if = ifno;
	args.rf_pkt = pkt;
	args.rf_ep = ep;
	args.rf_len = length;
	args.rf_hop = hopcnt;
	rawflip(&args);
	return args.rf_result;
}

flip_unicast(ifno, pkt, flags, dst, ep, length, ltime)
int ifno;
char * pkt;
int flags;
adr_p dst;                 
int ep;
f_size_t length;
interval ltime;
{
	struct rawflip_parms args;

	args.rf_opcode = RF_OPC_UNICAST;
	args.rf_if = ifno;
	args.rf_ep = ep;
	args.rf_pkt = pkt;
	args.rf_len = length;
	args.rf_dst = *dst;
	args.rf_flags = flags;
	args.rf_ltime = ltime;
	rawflip(&args);
	return args.rf_result;
}

flip_multicast(ifno, pkt, flags, dst, ep, length, n, ltime)
int ifno;
char * pkt;
int flags;
adr_p dst;                 
int ep;
f_size_t length;
int n;
interval ltime;
{
	struct rawflip_parms args;

	args.rf_opcode = RF_OPC_MULTICAST;
	args.rf_if = ifno;
	args.rf_ep = ep;
	args.rf_pkt = pkt;
	args.rf_len = length;
	args.rf_dst = *dst;
	args.rf_flags = flags;
	args.rf_ltime = ltime;
	args.rf_n = n;
	rawflip(&args);
	return args.rf_result;
}
