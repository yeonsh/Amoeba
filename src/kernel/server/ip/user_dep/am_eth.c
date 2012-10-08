/*	@(#)am_eth.c	1.1	91/11/25 13:15:00 */
/*
amoeba_dep/am_eth.c
*/

#include <stdlib.h>

#define port am_port_t
#include <amoeba.h>
#include <server/ip/tcpip.h>
#include <server/ip/eth_io.h>
#undef port

#include "nw_task.h"
#include "amoeba_incl.h"
#include "generic/buf.h"

#include "generic/assert.h"
#include "generic/eth.h"
#include "osdep_eth.h"
#include "generic/eth_int.h"
#include "generic/io.h"
#include "generic/sr.h"

#include "am_eth.h"

typedef struct eth_rec_param
{
	eth_port_t *erp_eth_port;
	ether_type_t erp_ether_type;
} eth_rec_param_t;

static void receive_thread ARGS(( void ));

static thread_nr;

#define RECEIVE_STACK	10240

void eth_init0()
{
	static char buf[256];
	capability server_cap;

	errstat error;
	eth_port_t *eth_port;
	nwio_ethopt_t ethopt;
	nwio_ethstat_t ethstat;

	thread_nr= 0;

	eth_port= &eth_port_table[0];

	if (!eth_port->etp_osdep.etp_ehook_name)
	{
		printf("Warning no capability for eth0\n");
		return;
	}
	error= name_lookup(eth_port->etp_osdep.etp_ehook_name, &server_cap);
	if (error != STD_OK)
		ip_panic(( "unable to lookup eth server: %s", err_why(error) ));

	error= tcpip_open(&server_cap, &eth_port->etp_osdep.etp_cap);
	if (error != STD_OK)
		ip_panic(( "unable to tcpip_open: %s", err_why(error) ));

	ethopt.nweo_flags= NWEO_COPY | NWEO_EN_LOC | NWEO_EN_BROAD | 
		NWEO_REMANY | NWEO_TYPEANY | NWEO_RWDATALL;
	error= eth_ioc_setopt(&eth_port->etp_osdep.etp_cap, &ethopt);
	if (error != STD_OK)
		ip_panic (( "unable to eth_ioc_setopt: %s", err_why(error) ));

	error= eth_ioc_getstat(&eth_port->etp_osdep.etp_cap, &ethstat);
	if (error != STD_OK)
		ip_panic (( "unable to eth_ioc_getstat: %s", err_why(error) ));

	eth_port->etp_ethaddr= ethstat.nwes_addr;

	printf("the ethernet address of this port is ");
	writeEtherAddr(&eth_port->etp_ethaddr);
	printf("\n");

	if (sr_add_minor (ETH_DEV0, eth_port- eth_port_table,
		eth_open, eth_close, eth_read, eth_write,
		eth_ioctl, eth_cancel)<0)
		ip_panic (( "can't sr_init" ));

	eth_port->etp_flags |= EPF_ENABLED;
	eth_port->etp_wr_pack= 0;
	eth_port->etp_rd_pack= 0;
	new_thread(receive_thread, RECEIVE_STACK);
	mu_lock(&mu_generic);
}

void eth_init_ehook_names()
{
	int i;

	for (i=0; i<ETH_PORT_NR; i++)
		eth_port_table[i].etp_osdep.etp_ehook_name= NULL;
}

void eth_set_ehook_name(minor, ehook_name)
int minor;
char *ehook_name;
{
	if (minor<0 || minor >= ETH_PORT_NR ||
		eth_port_table[minor].etp_osdep.etp_ehook_name)
	{
		ip_warning (( "no ethernet device %d" ));
		return;
	}
	eth_port_table[minor].etp_osdep.etp_ehook_name= ehook_name;
}

PRIVATE void receive_thread()
{
	eth_port_t *eth_port;
	acc_t *receive_buf;
	errstat error;
	int my_thread;

	my_thread= thread_nr++;

assert (my_thread >= 0 && my_thread < ETH_PORT_NR);

	eth_port= &eth_port_table[my_thread];

	for (;;)
	{
		receive_buf= bf_memreq(ETH_MAX_PACK_SIZE);
assert (!receive_buf->acc_next);

		mu_unlock(&mu_generic);
		error= tcpip_read(&eth_port->etp_osdep.etp_cap, 
			ptr2acc_data(receive_buf), ETH_MAX_PACK_SIZE);
		mu_lock(&mu_generic);

		if (ERR_STATUS(error))
			ip_panic(( "unable to tcpip_read: %s", 
				err_why(ERR_CONVERT(error)) ));
		receive_buf->acc_length= error;;
		eth_arrive(eth_port, receive_buf);
	}
}

int eth_get_stat(eth_port, eth_stat)
eth_port_t *eth_port;
eth_stat_t *eth_stat;
{
where(); printf("eth_get_stat not implemented\n");
	return NW_OK;
}

void eth_write_port(eth_port)
eth_port_t *eth_port;
{
	int i, pack_size;
	errstat error;
	acc_t *pack, *pack_ptr;

#if DEBUG
 { where(); printf("eth_write_port(&eth_port_table[%d], ..) called\n",
	eth_port-eth_port_table); }
#endif
assert (!(eth_port->etp_flags & EPF_WRITE_IP));

	eth_port->etp_flags |= EPF_WRITE_IP;

assert (eth_port->etp_wr_pack);

	pack= eth_port->etp_wr_pack;
	eth_port->etp_wr_pack= 0;

#if DEBUG && 256
 { where(); printf("bf_bufsize= %d\n", bf_bufsize(pack)); }
#endif
	if (pack->acc_next)
	{
#if DEBUG
 { where(); printf("compacting fragment\n"); }
#endif
		pack= bf_pack(pack);
					/* packet is too fragmented */
	}
#if DEBUG && 256
 { where(); printf("bf_bufsize= %d\n", bf_bufsize(pack));
 where(); printf("i= %d\n", i); }
#endif

assert (!pack->acc_next);

	pack_size= bf_bufsize(pack);

assert (pack_size >= ETH_MIN_PACK_SIZE);

#if DEBUG
 { eth_hdr_t *eth_hdr; eth_hdr= (eth_hdr_t *)ptr2acc_data(pack);
	printf("size: %d\n", sizeof(*eth_hdr));
	where(); printf("calling eh_xmit\n");
	where(); printf("src: "); writeEtherAddr(&eth_hdr->eh_src); 
	printf(" dst: "); writeEtherAddr(&eth_hdr->eh_dst); 
	printf(" proto: 0x%x, size %d\n", ntohs(eth_hdr->eh_proto), pack_size);
 }
#endif
	error= tcpip_write(&eth_port->etp_osdep.etp_cap, ptr2acc_data(pack),
		pack_size);
		
#if DEBUG
 { where(); printf("eh_xmit done\n"); }
#endif
#if DEBUG || 1
 if (error != pack_size)
 { where(); printf("error= %d\n", error); }
#endif

assert (error == pack_size);

	eth_arrive(eth_port, pack);

#if DEBUG && 256
 { where(); printf("write done\n"); }
#endif

	eth_port->etp_flags &= ~EPF_WRITE_COMPL;
	eth_port->etp_flags &= ~EPF_WRITE_IP;
assert (!(eth_port->etp_flags & (EPF_WRITE_IP|EPF_WRITE_SP|
	EPF_MORE2WRITE)));
}

void eth_chk_rec_conf (eth_port)
eth_port_t *eth_port;
{
where(); printf("not implemented\n");
#if 0
	eth_fd_t *eth_fd;
	int i, result;
	unsigned dl_flags;
	unsigned long flags= NWEO_NOFLAGS;
	static message mess, repl_mess;

#if DEBUG
 { where(); printf("chk_eth_rec_conf(&eth_port_table[%d])\n",
	eth_port-eth_port_table); }
#endif
	for (i=0, eth_fd= eth_fd_table; i<ETH_FD_NR; i++, eth_fd++)
	{
		if (!(eth_fd->ef_flags | ~(EFF_INUSE | EFF_OPTSET)))
			continue;
		if (eth_fd->ef_port != eth_port)
			continue;
		flags |= eth_fd->ef_ethopt.nweo_flags;
	}
	dl_flags= DL_NOMODE;
	if (flags & NWEO_EN_BROAD)
		dl_flags |= DL_BROAD_REQ;
	if (flags & NWEO_EN_MULTI)
		dl_flags |= DL_MULTI_REQ;
	if (flags & NWEO_EN_PROMISC)
		dl_flags |= DL_PROMISC_REQ;

	mess.m_type= DL_INIT;
	mess.DL_PORT= eth_port->etp_port;
	mess.DL_PROC= THIS_PROC;
	mess.DL_MODE= dl_flags;

	do
	{
		result= send (eth_port->etp_task, &mess);
		if (result == ELOCKED)	/* etp_task is sending to this task,
					   I hope */
		{
			if (receive (eth_port->etp_task, &repl_mess)< 0)
				ip_panic (( "unable to receive" ));
#if DEBUG
 { where(); printf("calling eth_int\n"); }
#endif
			eth_int (&repl_mess);
		}
	} while (result == ELOCKED);
	
	if (result < 0)
	{
		printf("got error %d\n", result);
		ip_panic (( "unable to send" ));
	}

	if (receive (eth_port->etp_task, &repl_mess) < 0)
		ip_panic (( "unable to receive" ));

	assert (repl_mess.m_type == DL_INIT_REPLY);
	if (repl_mess.m3_i1 != eth_port->etp_port)
	{
		printf("eth_init0: got reply for wrong port\n");
		return;
	}
#endif
}
