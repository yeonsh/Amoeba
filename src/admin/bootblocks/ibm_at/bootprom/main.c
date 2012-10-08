/*	@(#)main.c	1.4	96/03/07 14:08:40 */
/*
 * main.c
 *
 * Main routine and monitor loop
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */
#include "assert.h"
#include "param.h"
#include "types.h"
#include "packet.h"
#include "ether.h"
#include "inet.h"
#include "arp.h"
#include "tftp.h"

#define	bcd2dec(n)	((((n) & 0xF0) >> 4) * 10 + ((n) & 0x0F))

static char file[64];

/*
 * RARP/TFTP bootstrap loader consists of two phases. During the
 * first phase the loader tries to determine its IP address by
 * using the RARP protocol. The seconds phase consist of using
 * TFTP to download the program to be booted. First a TFTP read
 * file is tried with the first host that answered the RARP
 * request, if this fails a TFTP read file is broadcasted in the
 * hope that someone out there knows the file.
 */
main()
{
    ipaddr_t server = IP_BCASTADDR;
    int loaded = 0;

    /* initialize ethernet board, and random generator */
    printf("RARP/TFTP bootstrap loader\n\n");

    pkt_init();
    if (!eth_init()) {
	printf("No ethernet board found\n");
	halt();
    }
    srand(timer() ^ eth_myaddr[5]);

#ifdef MONITOR
    /*
     * When ever the user types CRTL BREAK we jump into
     * a special kind of monitor program.
     */
    printf("** Press <CTRL>-break to go to monitor **\n\n");
    if (brkhandler())
	monitor(server);
#endif

    /* actual RARP/TFTP bootstrap loader */
    if (rarp_getipaddress(&server)) {
	bootfile(file);
	if (!(loaded = tftp(server, ip_gateway, file)))
	    loaded = tftp(IP_BCASTADDR, ip_gateway, file);
    }

#ifdef MONITOR
    brkreset();
#endif
    eth_stop();

    if (loaded)
	execute();
    reboot();
}

#ifdef MONITOR

#define	CMD_GO		1
#define	CMD_HELP	2
#define	CMD_RESET	3
#define	CMD_AUTO	4
#define	CMD_RARP	5
#define	CMD_REBOOT	6
#define	CMD_DISKBOOT	7
#define	CMD_ADDRESS	8
#define	CMD_GATEWAY	9
#define	CMD_TFTP	10

struct commands {
    int	cmd_token;
    char *cmd_name;
    char *cmd_help;
} commands[] = {
    CMD_GO, 	  "go",		"                       start loaded binary",
    CMD_HELP,	  "help",	"                     this help mesage",
    CMD_RESET,	  "reset",	"                    reset ethernet board",
    CMD_AUTO,	  "auto",	"                     continue with auto boot",
    CMD_RARP,	  "rarp",	"                     get IP address",
    CMD_REBOOT,	  "reboot",	"                   hard reboot",
    CMD_DISKBOOT, "diskboot",	"                 soft reboot (from disk)",
    CMD_ADDRESS,  "address",	"[<addr>]          set IP address",
    CMD_GATEWAY,  "gateway",	"[<addr>]          set IP gateway",
    CMD_TFTP,	  "tftp",	"[<server>][:<file>]  TFTP download",
};

#define	NCOMMANDS	(sizeof(commands)/sizeof(struct commands))

/*
 * A very simple and terse monitor
 */
void
monitor(server)
    ipaddr_t server;
{
    char *argv[ARGVECSIZE];
    int loaded, argc;
    register int i;
    int token;
    ipaddr_t ip_convertaddr();

    loaded = 0;
    printf("\nRARP/TFTP monitor mode\n");
    for (;;) {
	printf("monitor: ");
	if ((argc = getline(argv, ARGVECSIZE)) > 0) {

	    for (token = -1, i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].cmd_name) == 0) {
		    token = commands[i].cmd_token;
		    break;
		}
	    }
	    switch (token) {
	    case CMD_HELP:
		for (i = 0; i < NCOMMANDS; i++)
		    printf("%s %s\n",
			commands[i].cmd_name, commands[i].cmd_help);
		break;
	    case CMD_GO:
		if (!loaded) goto complain;
		brkreset();
		eth_stop();
		execute();
		break;
	    case CMD_RESET:
		pkt_init();
		eth_reset();
		break;
	    case CMD_REBOOT:
		brkreset();
		eth_stop();
		reboot();
		break;
	    case CMD_DISKBOOT:
		brkreset();
		eth_stop();
		diskboot();
		break;
	    case CMD_AUTO:
		return;
	    case CMD_RARP:
		(void) rarp_getipaddress(&server);
		break;
	    case CMD_ADDRESS:
		if (argc != 2) {
		    printf("My IP address is ");
		    ip_printaddr(ip_myaddr);
		    printf("\n");
		} else
		    ip_myaddr = ip_convertaddr(argv[1]);
		break;
	    case CMD_GATEWAY:
		if (argc != 2) {
		    printf("Gateway IP address is ");
		    ip_printaddr(ip_gateway);
		    printf("\n");
		} else
		    ip_gateway = ip_convertaddr(argv[1]);
		break;
	    case CMD_TFTP:
		if (ip_myaddr == IP_ANYADDR)
		    goto complain;
		bootfile(file);
		if (argc == 2) {
		    register char *p, *q;
		    ipaddr_t addr;

		    for (p = argv[1]; *p; p++) {
			if (p[0] == ':' && p[1]) {
			    for (*p++ = '\0', q = file; *p; )
				*q++ = *p++;
			    *q = '\0';
			    break;
			}
		    }
		    if ((addr = ip_convertaddr(argv[1])) != IP_ANYADDR)
			server = addr;
		}
		loaded = tftp(server, ip_gateway, file);
		if (!loaded)
		    server = IP_BCASTADDR;
		break;
	    default:
		goto complain;
	    }
	} else
complain:
	    printf("invalid or incorrect command\n");
    }
}

char *
number(s, n)
    char *s;
    uint8 *n;
{
    for (*n = 0; *s >= '0' && *s <= '9'; s++)
	*n = (*n * 10) + *s - '0';
    return s;
}

ipaddr_t
ip_convertaddr(p)
    char *p;
{
    inetaddr_t addr;

    if (p == (char *)0 || *p == '\0')
	return IP_ANYADDR;
    p = number(p, &addr.s.a0);
    if (*p == '\0' || *p++ != '.')
	return IP_ANYADDR;
    p = number(p, &addr.s.a1);
    if (*p == '\0' || *p++ != '.')
	return IP_ANYADDR;
    p = number(p, &addr.s.a2);
    if (*p == '\0' || *p++ != '.')
	return IP_ANYADDR;
    p = number(p, &addr.s.a3);
    if (*p != '\0')
	return IP_ANYADDR;
    return addr.a;
}

int
getline(argv, argvsize)
    char **argv;
    int argvsize;
{
    register char *p, ch;
    static char line[128];
    int argc;

    /*
     * Read command line, implement some simple editing features
     */
    p = line;
    while ((ch = getchar()) != '\r' && ch != '\n' && (p-line) < sizeof(line)) {
	if (ch == '\b') {
	     if (p > line) {
		p--;
	     	printf(" \b");
	     }
	} else
	    *p++ = ch;
    }
    *p = '\0';

    /*
     * Break command line up into an argument vector
     */
    argc = 0;
    for (p = line; *p == ' ' || *p == '\t'; p++)
	/* skip white spaces */;
    while (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\0') {
	if (argc > argvsize) break;
	argv[(argc)++] = p;
	for (; *p != ' ' && *p != '\t' && *p != '\n' && *p != '\0'; p++)
	    /* skip word */;
	if (*p != '\0') *p++ ='\0';
	for (; *p == ' ' || *p == '\t'; p++)
	    /* skip white spaces */;
    }
    argv[(argc)] = (char *)0;
    return argc;
}

int
strcmp(s, t)
    register char *s, *t;
{
    while (*s == *t++)
	if (*s++ == '\0') return 0;
    return *s - *--t;
}
#endif /* MONITOR */

char
tohex(n)
    int n;
{
    n &= 0x0F;
    return n >= 10 ? n - 10 + 'A' : n + '0';
}

/*
 * Construct a define boot file name. We use more or less
 * the same convention as SUN, a hex representation of the
 * IP address followed by an architecture code (.iX86 in
 * our case).
 */
void
bootfile(name)
    char *name;
{
    inetaddr_t ip;

    ip.a = ip_myaddr;
    name[0] = tohex(ip.s.a0 >> 4);
    name[1] = tohex(ip.s.a0);
    name[2] = tohex(ip.s.a1 >> 4);
    name[3] = tohex(ip.s.a1);
    name[4] = tohex(ip.s.a2 >> 4);
    name[5] = tohex(ip.s.a2);
    name[6] = tohex(ip.s.a3 >> 4);
    name[7] = tohex(ip.s.a3);
    name[8] = '.';
    name[9] = 'i';
    name[10] = 'X';
    name[11] = '8';
    name[12] = '6';
    name[13] = '\0';
}

/*
 * Return the current time in seconds
 */
uint32
timer()
{
    int sec, min, hour;

    /* BIOS time is stored in bcd */
    getrtc(&sec, &min, &hour);
    sec = bcd2dec(sec);
    min = bcd2dec(min);
    hour = bcd2dec(hour);
    return ((uint32)hour * 3600L) + ((uint32)min * 60L) + (uint32)sec;
}

/*
 * Simple random generator
 */
static uint32 next = 1;

void
srand(seed)
    unsigned int seed;
{
    next = seed;
}

int
rand()
{
    next = next * 1103515245 + 12345;
    return (unsigned int)(next / 65536) % 32768;
}

#if 1
/* XXX stupid me: cannot write a proper bcmp in assembler */
bcmp(p, q, n)
    char *p, *q;
    int n;
{
    while (n--)
	if (*p++ != *q++)
	    return 1;
    return 0;
}
#endif
