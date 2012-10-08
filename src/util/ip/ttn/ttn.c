/*	@(#)ttn.c	1.5	96/02/27 13:09:46 */
/*
ttn.c
*/

#ifndef DEBUG
#define DEBUG	0
#endif

#define _POSIX_SOURCE 1

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sgtty.h>

#define port am_port_t
#define command am_command_t
#include <amoeba.h>
#include <ampolicy.h>
#include <amtools.h>
#include <stderr.h>
#include <thread.h>
#undef port
#undef command

#include <module/name.h>

#include <server/ip/types.h>
#include <server/ip/tcpip.h>
#include <server/ip/tcp_io.h>
#include <server/ip/hton.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/inet.h>
#include <server/ip/gen/netdb.h>
#include <server/ip/gen/tcp.h>
#include <server/ip/gen/tcp_io.h>

#include "ttn.h"
#if DEBUG
#include <server/ip/debug.h>
#endif

int main _ARGS(( int argc, char *argv[] ));
static void screen _ARGS(( void ));
static void keyboard _ARGS(( void ));
static void keyb_thread _ARGS(( char *param, int psize ));
static bufsize process_opt _ARGS(( char *bp, int count ));
static void do_option _ARGS(( int optsrt ));
static void dont_option _ARGS(( int optsrt ));
static void will_option _ARGS(( int optsrt ));
static void wont_option _ARGS(( int optsrt ));
static bufsize tcpip_writeall _ARGS(( capability *chan_cap, char *buffer,
	int buf_size ));
static bufsize sb_termtype _ARGS(( char *sb, int count ));
static void usage _ARGS(( void ));


#define KEYB_STACK	10240

static char *prog_name;
static char *term_env;
capability chan_cap;

main(argc, argv)
int argc;
char *argv[];
{
	struct hostent *hostent;
	struct servent *servent;
	ipaddr_t host;
	tcpport_t port;
	nwio_tcpconf_t tcpconf;
	nwio_tcpcl_t tcpconnopt;
	int count, result;
	static char buffer[1024];
	struct sgttyb sgttyb;
	char *tcp_cap_name, *host_name, *port_name;
	int arg_ind;
	capability tcp_cap;

	prog_name= argv[0];
	tcp_cap_name= NULL;
	for (arg_ind= 1; arg_ind<argc; arg_ind++)
	{
		if (!strcmp(argv[arg_ind], "-tcp"))
		{
			if (tcp_cap_name)
				usage();
			arg_ind++;
			if (arg_ind >= argc)
				usage();
			tcp_cap_name= argv[arg_ind];
			continue;
		}
		if (!strcmp(argv[arg_ind], "-?"))
			usage();
		break;
	}
	if (arg_ind >= argc)
		usage();
	host_name= argv[arg_ind];
	arg_ind++;
	if (arg_ind<argc)
	{
		port_name= argv[arg_ind];
		arg_ind++;
	}
	else
		port_name= NULL;
	
	if (arg_ind != argc)
		usage();
	
	if (!tcp_cap_name)
	{
		tcp_cap_name= getenv("TCP_SERVER");
		if (tcp_cap_name && !tcp_cap_name[0])
			tcp_cap_name= NULL;
	}
	if (!tcp_cap_name)
		tcp_cap_name= TCP_SVR_NAME;

#if DEBUG
 { where(); printf("looking up %s\n", host_name); endWhere(); }
#endif
	hostent= gethostbyname(host_name);
	if (!hostent)
	{
		fprintf(stderr, "%s: unknown host '%s'\r\n",
				prog_name, host_name);
		exit(1);
	}
	host= *(ipaddr_t *)(hostent->h_addr);

	if (!port_name)
		port= htons(TCPPORT_TELNET);
	else
	{
		servent= getservbyname (port_name, "tcp");
		if (!servent)
		{
			port= htons(strtol (port_name, (char **)0, 0));
			if (!port)
			{
				fprintf(stderr, "%s: unknown port (%s)\r\n",
					prog_name, port_name);
				exit(1);
			}
		}
		else
			port= (tcpport_t)(servent->s_port);
	}

	printf("connecting to %s %u\r\n", inet_ntoa(host), ntohs(port));

	result= name_lookup(tcp_cap_name, &tcp_cap);
	if (result<0)
	{
		fprintf(stderr, "%s: unable to lookup %s: %s\n", prog_name,
			tcp_cap_name, tcpip_why(result));
		exit(1);
	}
	result= tcpip_open (&tcp_cap, &chan_cap);
	if (result<0)
	{
		fprintf(stderr, "%s: unable to tcpip_open: %s\n", prog_name,
			tcpip_why(result));
		exit(1);
	}

	tcpconf.nwtc_flags= NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
	tcpconf.nwtc_remaddr= host;
	tcpconf.nwtc_remport= port;

	result= tcp_ioc_setconf (&chan_cap, &tcpconf);
	if (result<0)
	{
		fprintf(stderr, "%s: unable to  tcp_ioc_setconf: %s\n",
			prog_name, tcpip_why(result));
		std_destroy(&chan_cap);
		exit(1);
	}

	tcpconnopt.nwtcl_flags= 0;

	do
	{
		result= tcp_ioc_connect (&chan_cap, &tcpconnopt);
		if (result == EAGAIN)
		{
			fprintf(stderr,"%s: got EAGAIN, sleeping(1s)\r\n",
				prog_name);
			sleep(1);
		}
	} while (result == EAGAIN);
	if (result != 0)
	{
		fprintf(stderr, "%s: unable to tcp_ioc_connect: %s\r\n",
			prog_name, tcpip_why(result));
		std_destroy(&chan_cap);
		exit(1);
	}
	printf("connect succeeded !!!!\r\n");
	result= thread_newthread (keyb_thread, KEYB_STACK, (char *)0, 0);
	if (!result)
	{
		fprintf(stderr, "%s: unable to start new thread\n",
			prog_name);
		std_destroy(&chan_cap);
		exit(1);
	}
	screen();
	gtty (1, &sgttyb);
	sgttyb.sg_flags |= ECHO|CRMOD;
	sgttyb.sg_flags &= ~RAW;
	stty (1, &sgttyb);
	std_destroy(&chan_cap);
	exit(0);
}

static void keyb_thread (param, psize)
char *param;
int psize;
{
	struct sgttyb sgttyb;

	keyboard();
	gtty (1, &sgttyb);
	sgttyb.sg_flags |= ECHO|CRMOD;
	sgttyb.sg_flags &= ~RAW;
	stty (1, &sgttyb);
	std_destroy(&chan_cap);
	exit(0);
}

static void screen()
{
	char buffer[1024], *bp, *iacptr;
	int count, optsize;

	for (;;)
	{
		count= tcpip_read (&chan_cap, buffer, sizeof(buffer));
#if DEBUG
 { where(); fprintf(stderr, "tcpip_read %d bytes\r\n", count); endWhere(); }
#endif
		if (ERR_STATUS(count))
		{
			fprintf (stderr, "%s: unable to tcpip_read: %s", 
				prog_name, tcpip_why(count));
			return;
		}
		if (!count)
			return;
		bp= buffer;
		do
		{
			iacptr= memchr (bp, IAC, count);
			if (!iacptr)
			{
#if DEBUG && 0
 { int i;  where(); fprintf(stderr, "no options, wrinting %d bytes\n", count); 
	for (i=0; i<count; i++) printf("%d ", bp[i]); printf("\n");
	endWhere(); }
#endif
				write(1, bp, count);
				count= 0;
			}
			if (iacptr && iacptr>bp)
			{
#if DEBUG 
 { where(); fprintf(stderr, "iacptr-bp= %d\r\n", iacptr-bp); endWhere(); }
#endif
				write(1, bp, iacptr-bp);
				count -= (iacptr-bp);
				bp= iacptr;
				continue;
			}
			if (iacptr)
			{
assert (iacptr == bp);
#if DEBUG
 { where(); fprintf(stderr, "processing option\r\n"); endWhere(); }
#endif
				optsize= process_opt(bp, count);
#if DEBUG
 { where(); fprintf(stderr, "process_opt(...)= %d\r\n", optsize); endWhere(); }
#endif
				if (ERR_STATUS(optsize))
					return;
assert (optsize);
				bp += optsize;
				count -= optsize;
			}
		} while (count);
	}
}

static void keyboard()
{
	nwio_tcpatt_t nwio_tcpatt;
	char buffer[1024];
	struct sgttyb sgttyb;
	
	int count;
	nwio_tcpatt.nwta_flags= 0;

	for (;;)
	{
		count= read (0, buffer, 1 /* sizeof(buffer) */);
		if (count<0)
		{
			perror("read");
/*
			fprintf(stderr, "%s: read: %s\r\n", prog_name,
				strerror(errno));
 */
			return;
		}
		if (!count)
			return;
#if DEBUG && 0
 { where(); fprintf(stderr, "writing %d bytes\r\n", count); endWhere(); }
#endif
		count= tcpip_write(&chan_cap, buffer, count);
		if (buffer[0] == '\r')
			tcpip_write(&chan_cap, "\n", 1);
		if (ERR_STATUS(count))
		{
			fprintf (stderr, "%s: unable to tcpip_write: %s\r\n",
				prog_name, tcpip_why(count));
			return;
		}
	}
}

#define next_char(var) \
	if (offset<count) { (var) = bp[offset++]; } \
	else if (ERR_STATUS(result= tcpip_read(&chan_cap, (char *)&(var), 1))) \
	{ fprintf (stderr, "%s: unable to tcpip_read: %s\r\n", prog_name, \
	tcpip_why(result)); return -1; }

static bufsize process_opt (bp, count)
char *bp;
int count;
{
	unsigned char iac, command, optsrt, sb_command;
	int offset, result;

#if DEBUG && 0
 { where(); fprintf(stderr, "process_opt(bp= 0x%x, count= %d)\r\n",
	bp, count); endWhere(); }
#endif

	offset= 0;
assert (count);
	next_char(iac);
assert (iac == IAC);
	next_char(command);
	switch(command)
	{
	case IAC_NOP:
		break;
	case IAC_DataMark:
fprintf(stderr, "got a DataMark\r\n");
		break;
	case IAC_BRK:
fprintf(stderr, "got a BRK\r\n");
		break;
	case IAC_IP:
fprintf(stderr, "got a IP\r\n");
		break;
	case IAC_AO:
fprintf(stderr, "got a AO\r\n");
		break;
	case IAC_AYT:
fprintf(stderr, "got a AYT\r\n");
		break;
	case IAC_EC:
fprintf(stderr, "got a EC\r\n");
		break;
	case IAC_EL:
fprintf(stderr, "got a EL\r\n");
		break;
	case IAC_GA:
fprintf(stderr, "got a GA\r\n");
		break;
	case IAC_SB:
		next_char(sb_command);
		switch (sb_command)
		{
		case OPT_TERMTYPE:
#if DEBUG && 0
fprintf(stderr, "got SB TERMINAL-TYPE\r\n");
#endif
			result= sb_termtype(bp+offset, count-offset);
			if (ERR_STATUS(result))
				return result;
			else
				return result+offset;
		default:
fprintf(stderr, "got an unknown SB (%d) (skiping)\r\n", sb_command);
			for (;;)
			{
				next_char(iac);
				if (iac != IAC)
					continue;
				next_char(optsrt);
				if (optsrt == IAC)
					continue;
if (optsrt != IAC_SE)
	fprintf(stderr, "got IAC %d\r\n", optsrt);
				break;
			}
		}
		break;
	case IAC_WILL:
		next_char(optsrt);
		will_option(optsrt);
		break;
	case IAC_WONT:
		next_char(optsrt);
		wont_option(optsrt);
		break;
	case IAC_DO:
		next_char(optsrt);
		do_option(optsrt);
		break;
	case IAC_DONT:
		next_char(optsrt);
		dont_option(optsrt);
		break;
	case IAC:
fprintf(stderr, "got a IAC\r\n");
		break;
	default:
fprintf(stderr, "got unknown command (%d)\r\n", command);
	}
	return offset;
}

static void do_option (optsrt)
int optsrt;
{
	unsigned char reply[3], *rp;
	int count, result;

	switch (optsrt)
	{
	case OPT_TERMTYPE:
		if (WILL_terminal_type)
			return;
		if (!WILL_terminal_type_allowed)
		{
			reply[0]= IAC;
			reply[1]= IAC_WONT;
			reply[2]= optsrt;
		}
		else
		{
			WILL_terminal_type= TRUE;
			term_env= getenv("TERM");
			if (!term_env)
				term_env= "unknown";
			reply[0]= IAC;
			reply[1]= IAC_WILL;
			reply[2]= optsrt;
		}
		break;
	default:
#if DEBUG
 { where(); fprintf(stderr, "got a DO (%d)\r\n", optsrt); endWhere(); }
#endif
#if DEBUG
 { where(); fprintf(stderr, "WONT (%d)\r\n", optsrt); endWhere(); }
#endif
		reply[0]= IAC;
		reply[1]= IAC_WONT;
		reply[2]= optsrt;
		break;
	}
	result= tcpip_writeall(&chan_cap, (char *)reply, 3);
	if (ERR_STATUS(result))
	{
		fprintf (stderr, "%s: unable to tcpip_write: %s\n",
			prog_name, tcpip_why(result));
	}
}

static void will_option (optsrt)
int optsrt;
{
	unsigned char reply[3], *rp;
	int count, result;

	switch (optsrt)
	{
	case OPT_ECHO:
		if (DO_echo)
			break;
		if (!DO_echo_allowed)
		{
			reply[0]= IAC;
			reply[1]= IAC_DONT;
			reply[2]= optsrt;
		}
		else
		{
			struct sgttyb sgttyb;
			gtty (1, &sgttyb);
			sgttyb.sg_flags &= ~(ECHO|CRMOD);
			sgttyb.sg_flags |= RAW;
			stty (1, &sgttyb);
			DO_echo= TRUE;
			reply[0]= IAC;
			reply[1]= IAC_DO;
			reply[2]= optsrt;
		}
		result= tcpip_writeall(&chan_cap, (char *)reply, 3);
		if (ERR_STATUS(result))
			fprintf(stderr, "%s: unable to tcpip_write: %s\n",
				prog_name, tcpip_why(result));
		break;
	case OPT_SUPP_GA:
		if (DO_suppress_go_ahead)
			break;
		if (!DO_suppress_go_ahead_allowed)
		{
			reply[0]= IAC;
			reply[1]= IAC_DONT;
			reply[2]= optsrt;
		}
		else
		{
			DO_suppress_go_ahead= TRUE;
			reply[0]= IAC;
			reply[1]= IAC_DO;
			reply[2]= optsrt;
		}
		result= tcpip_writeall(&chan_cap, (char *)reply, 3);
		if (ERR_STATUS(result))
			fprintf(stderr, "%s: unable to tcpip_write: %s\n",
				prog_name, tcpip_why(result));
		break;
	default:
#if DEBUG
 { where(); fprintf(stderr, "got a WILL (%d)\r\n", optsrt); endWhere(); }
#endif
#if DEBUG
 { where(); fprintf(stderr, "DONT (%d)\r\n", optsrt); endWhere(); }
#endif
		reply[0]= IAC;
		reply[1]= IAC_DONT;
		reply[2]= optsrt;
		result= tcpip_writeall(&chan_cap, (char *)reply, 3);
		if (ERR_STATUS(result))
			fprintf (stderr, "%s: unable to tcpip_write: %d\n",
				prog_name, tcpip_why(result));
		break;
	}
}

static bufsize tcpip_writeall (chan_fd, buffer, buf_size)
capability *chan_fd;
char *buffer;
int buf_size;
{
	int result;

	while (buf_size)
	{
		result= tcpip_write (chan_fd, buffer, buf_size);
		if (ERR_STATUS(result))
			return result;
assert (result <= buf_size);
		buffer += result;
		buf_size -= result;
	}
	return STD_OK;
}

static void dont_option (optsrt)
int optsrt;
{
	unsigned char reply[3], *rp;
	int count, result;

	switch (optsrt)
	{
	default:
#if DEBUG
 { where(); fprintf(stderr, "got a DONT (%d)\r\n", optsrt); endWhere(); }
#endif
		break;
	}
}

static void wont_option (optsrt)
int optsrt;
{
	unsigned char reply[3], *rp;
	int count, result;

	switch (optsrt)
	{
	default:
#if DEBUG
 { where(); fprintf(stderr, "got a WONT (%d)\r\n", optsrt); endWhere(); }
#endif
		break;
	}
}

static bufsize sb_termtype (bp, count)
char *bp;
int count;
{
	unsigned char command, iac, optsrt;
	unsigned char buffer[4];
	int offset, result;

	offset= 0;
	next_char(command);
	if (command == TERMTYPE_SEND)
	{
		buffer[0]= IAC;
		buffer[1]= IAC_SB;
		buffer[2]= OPT_TERMTYPE;
		buffer[3]= TERMTYPE_IS;
		result= tcpip_writeall(&chan_cap, (char *)buffer,4);
		if (ERR_STATUS(result))
			return result;
		count= strlen(term_env);
		if (!count)
		{
			term_env= "unknown";
			count= strlen(term_env);
		}
		result= tcpip_writeall(&chan_cap, term_env, count);
		if (result<0)
			return result;
		buffer[0]= IAC;
		buffer[1]= IAC_SE;
		result= tcpip_writeall(&chan_cap, (char *)buffer,2);
		if (ERR_STATUS(result))
			return result;

	}
	else
	{
#if DEBUG
 { where(); endWhere(); }
#endif
		fprintf(stderr, "got an unknown command (skiping)\r\n");
	}
	for (;;)
	{
		next_char(iac);
		if (iac != IAC)
			continue;
		next_char(optsrt);
		if (optsrt == IAC)
			continue;
		if (optsrt != IAC_SE)
		{
#if DEBUG
 { where(); endWhere(); }
#endif
			fprintf(stderr, "got IAC %d\r\n", optsrt);
		}
		break;
	}
	return offset;
}

static void usage()
{
	fprintf(stderr, "USAGE: %s [-tcp <tcp capability>] host [port]\n",
		prog_name);
	exit(1);
}
