/*	@(#)tcpip.c	1.2	94/04/07 11:04:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
stubs/ip/tcpip.c
*/

#include <amtools.h>

#include <server/ip/tcpip.h>

#define NOTFOUND_RETRIES	4	/* No. of retries when RPC_NOTFOUND
					 * is returned.
					 */

errstat tcpip_open (tcpip_cap, chan_cap)
capability *tcpip_cap, *chan_cap;
{
	bufsize	repl_size;
	header trans_header;

	trans_header.h_port= tcpip_cap->cap_port;
	trans_header.h_priv= tcpip_cap->cap_priv;
	trans_header.h_command= TCPIP_OPEN;
	trans_header.h_size= 0;
	repl_size= trans(&trans_header, 0, 0, &trans_header, 0, 0);

	if (ERR_STATUS(repl_size))
		return ERR_CONVERT(repl_size);

	if ((short)trans_header.h_status < 0)
		return (short)trans_header.h_status;

	chan_cap->cap_port= trans_header.h_port;
	chan_cap->cap_priv= trans_header.h_priv;
	return (short)trans_header.h_status;
}

errstat tcpip_mkcap (tcpip_cap, obj, cap)
capability *tcpip_cap;
objnum obj;
capability *cap;
{
	bufsize	repl_size;
	header trans_header;

	trans_header.h_port= tcpip_cap->cap_port;
	trans_header.h_priv= tcpip_cap->cap_priv;
	trans_header.h_command= TCPIP_MKCAP;
	trans_header.h_size= 0;
	trans_header.h_extra= obj;
	repl_size= trans(&trans_header, 0, 0, &trans_header, 0, 0);

	if (ERR_STATUS(repl_size))
		return ERR_CONVERT(repl_size);

	if ((short)trans_header.h_status < 0)
		return (short)trans_header.h_status;

	cap->cap_port= trans_header.h_port;
	cap->cap_priv= trans_header.h_priv;
	return (short)trans_header.h_status;
}

bufsize tcpip_read (chan_cap, buf, size)
capability *chan_cap;
char *buf;
size_t size;
{
	header trans_header;
	bufsize repl_size;
	int i;

#if DEBUG & 2
 { where(); printf("tcpip_read(chan_cap= 0x%x, buf= 0x%x, size= %d)\n",
	chan_cap, buf, size); endWhere(); }
#endif
	for (i= 0; i<NOTFOUND_RETRIES; i++)
	{
		trans_header.h_port= chan_cap->cap_port;
		trans_header.h_priv= chan_cap->cap_priv;
		trans_header.h_command= TCPIP_READ;
		trans_header.h_size= size;
		repl_size= trans(&trans_header, 0, 0, &trans_header, buf, size);
#if DEBUG & 2
 { where(); fprintf(stderr, "trans: %d, %d\n", repl_size,
	trans_header.h_status); endWhere(); }
#endif
		if (!ERR_STATUS(repl_size) || 
			ERR_CONVERT(repl_size) != RPC_NOTFOUND)
		{
			break;
		}
	}
	if (ERR_STATUS(repl_size))
	{
#if DEBUG
 { where(); fprintf(stderr, "repl_size is error (%d)\n", repl_size);
	endWhere(); }
#endif
		return ERR_CONVERT(repl_size);
	}
	if ((short)trans_header.h_status < 0)
	{
#if DEBUG
 { where(); fprintf(stderr, "h_status < 0\n"); endWhere(); }
#endif
		return (short)trans_header.h_status;
	}
	if (repl_size != trans_header.h_size)
	{
#if DEBUG
 { where(); fprintf(stderr, "repl_size != h_size\n"); endWhere(); }
#endif
		return STD_ARGBAD;
	}
	return (short)trans_header.h_status;
}

bufsize tcpip_write (chan_cap, buf, size)
capability *chan_cap;
char *buf;
size_t size;
{
	header trans_header;
	bufsize repl_size;
	int i;

#if DEBUG & 2
 { where(); printf("tcpip_write(chan_cap= 0x%x, buf= 0x%x, size= %d)\n",
	chan_cap, buf, size); endWhere(); }
#endif
	for (i=0; i<NOTFOUND_RETRIES; i++)
	{
		trans_header.h_port= chan_cap->cap_port;
		trans_header.h_priv= chan_cap->cap_priv;
		trans_header.h_command= TCPIP_WRITE;
		trans_header.h_size= size;
		repl_size= trans(&trans_header, buf, size, &trans_header, 0, 0);
#if DEBUG & 2
 { where(); fprintf(stderr, "trans: %d, %d\n", repl_size,
	trans_header.h_status); endWhere(); }
#endif
		if (!ERR_STATUS(repl_size) || 
			ERR_CONVERT(repl_size) != RPC_NOTFOUND)
		{
			break;
		}
	}
	if (ERR_STATUS(repl_size))
	{
#if DEBUG
 { where(); fprintf(stderr, "repl_size is error\n"); endWhere(); }
#endif
		return ERR_CONVERT(repl_size);
	}
	if ((short)trans_header.h_status < 0)
	{
#if DEBUG
 { where(); fprintf(stderr, "h_status < 0\n"); endWhere(); }
#endif
		return (short)trans_header.h_status;
	}
	if (trans_header.h_size != 0)
	{
#if DEBUG
 { where(); fprintf(stderr, "h_size != 0\n"); endWhere(); }
#endif
		return STD_ARGBAD;
	}
	return (short)trans_header.h_status;
}

errstat tcpip_keepalive (chan_cap, respite)
capability *chan_cap;
int *respite;
{
	header trans_header;
	bufsize repl_size;
	int i;

#if DEBUG & 2
 { where(); printf("tcpip_keepalive(chan_cap= 0x%x, respite= 0x%x)\n",
	chan_cap, respite); endWhere(); }
#endif

	for(i= 0; i<NOTFOUND_RETRIES; i++)
	{
		trans_header.h_port= chan_cap->cap_port;
		trans_header.h_priv= chan_cap->cap_priv;
		trans_header.h_command= TCPIP_KEEPALIVE;
		repl_size= trans(&trans_header, 0, 0, &trans_header, 0, 0);
		if (!ERR_STATUS(repl_size) || 
			ERR_CONVERT(repl_size) != RPC_NOTFOUND)
		{
			break;
		}
	}

#if DEBUG & 2
 { where(); fprintf(stderr, "trans: %d, %d\n", repl_size,
	trans_header.h_status); endWhere(); }
#endif
	if (ERR_STATUS(repl_size))
	{
#if DEBUG
 { where(); fprintf(stderr, "repl_size is error\n"); endWhere(); }
#endif
		return ERR_CONVERT(repl_size);
	}
	if ((short)trans_header.h_status < 0)
	{
#if DEBUG
 { where(); fprintf(stderr, "h_status < 0\n"); endWhere(); }
#endif
		return (short)trans_header.h_status;
	}
	if (respite)
		*respite= trans_header.h_offset;
	return (short)trans_header.h_status;
}
