/*	@(#)rarp.c	1.4	96/02/27 10:14:46 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
admin/ip/rarp.c
*/

#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <amtools.h>
#include <ampolicy.h>
#include <thread.h>

#include <module/mutex.h>
#include <module/host.h>
#include <server/bullet/bullet.h>

#include <server/ip/hton.h>
#include <server/ip/types.h>
#include <server/ip/tcpip.h>
#include <server/ip/eth_io.h>
#include <server/ip/ip_io.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/inet.h>
#include <server/ip/gen/ip_io.h>
#include <server/ip/gen/if_ether.h>
#include <server/ip/gen/ether.h>
#include <server/ip/gen/eth_io.h>
#include <server/ip/gen/socket.h>
#include <server/ip/gen/netdb.h>

#define NRARPTHREAD		10

#define SUN4_TFTPBOOT_BUG	1	/* A sun4 does repond to arp requests
					 * during tftp boot
					 * We supply the arp reply our selves
					 */

typedef struct rarp46
{
	ether_addr_t a46_dstaddr;
	ether_addr_t a46_srcaddr;
	ether_type_t a46_ethtype;
	union
	{
		struct
		{
			u16_t a_hdr, a_pro;
			u8_t a_hln, a_pln;
			u16_t a_op;
			ether_addr_t a_sha;
			u8_t a_spa[4];
			ether_addr_t a_tha;
			u8_t a_tpa[4];
		} a46_data;
		char    a46_dummy[ETH_MIN_PACK_SIZE-ETH_HDR_SIZE];
	} a46_data;
} rarp46_t;

#define a46_hdr a46_data.a46_data.a_hdr
#define a46_pro a46_data.a46_data.a_pro
#define a46_hln a46_data.a46_data.a_hln
#define a46_pln a46_data.a46_data.a_pln
#define a46_op  a46_data.a46_data.a_op
#define a46_sha a46_data.a46_data.a_sha
#define a46_spa a46_data.a46_data.a_spa
#define a46_tha a46_data.a46_data.a_tha
#define a46_tpa a46_data.a46_data.a_tpa

#define RARP_ETHERNET	1

#define ARP_REPLY	2
#define RARP_REQUEST	3
#define RARP_REPLY	4

char *prog_name;
ether_addr_t my_ether_addr;
ipaddr_t my_ipaddr;
static int verbose;
static int docache;
static mutex print_mu;

int main _ARGS(( int argc, char *argv[]  ));
static int rarp _ARGS(( ether_addr_t *ea, ipaddr_t *ia ));
static int cache_rarp _ARGS(( ether_addr_t *ea, ipaddr_t *ia ));
static void usage _ARGS(( void ));
static errstat open_channel _ARGS(( capability *eth_cap, capability *chan_cap,
	long eth_flags, int eth_type, ether_addr_t *eth_addr ));
static void svr_thread _ARGS(( char *param, int size ));
static int cache_init _ARGS(( void ));

char *eth_cap_name, *chan_cap_name;
capability eth_cap;
mutex thread_mutex;
ipaddr_t ip_net, ip_netmask;

static errstat
open_channel(eth_cap, chan_cap, eth_flags, eth_type, eth_addr)
capability *eth_cap;
capability *chan_cap;
long eth_flags;
int eth_type;
ether_addr_t *eth_addr;
{
    nwio_ethopt_t ethopt;
    nwio_ethstat_t ethstat;
    errstat result;

    result = tcpip_open (eth_cap, chan_cap);
    if (result != STD_OK)
    {
	fprintf(stderr, "%s: tcpip_open failed (%s)\n",
		prog_name, err_why(result));
	return result;
    }

    ethopt.nweo_flags = eth_flags;
    ethopt.nweo_type = eth_type;

    result = eth_ioc_setopt (chan_cap, &ethopt);
    if (result != STD_OK)
    {
	fprintf(stderr, "%s: eth_ioc_setopt failed (%s)\n",
		prog_name, err_why(result));
	return result;
    }

    result = eth_ioc_getstat (chan_cap, &ethstat);
    if (result != STD_OK)
    {
	fprintf(stderr, "%s: eth_ioc_getstat failed (%s)\n",
		prog_name, err_why(result));
	return result;
    }
    *eth_addr = ethstat.nwes_addr;

    return STD_OK;
}


/*ARGSUSED*/
static void svr_thread(param, size)
char *param;
{
	static int threadnogen;
	int threadno;
	errstat result;
	capability chan_cap, old_chan_cap;
	rarp46_t *rarp_ptr;
#ifdef SUN4_TFTPBOOT_BUG
	capability chan_cap1;
#endif
	unsigned char packet[ETH_MAX_PACK_SIZE];
	ether_addr_t ether;
	char chan_cap_name_number[100];
	ipaddr_t reply_ipaddr;

	mu_lock(&thread_mutex);
	threadno = ++threadnogen;

	result = open_channel(&eth_cap, &chan_cap, 
		     NWEO_SHARED | NWEO_EN_LOC | NWEO_EN_BROAD | NWEO_TYPESPEC,
		     ETH_RARP_PROTO, &ether);
	if (result != STD_OK) {
	    fprintf(stderr, "%s: fatal: open_channel (1) failed\n", prog_name);
	    exit(1);
	}

	if (chan_cap_name)
	{
		sprintf(chan_cap_name_number, "%s:%03d",
			chan_cap_name, threadno);
		result= name_lookup(chan_cap_name_number, &old_chan_cap);
		if (result == STD_OK)
		{
			std_destroy(&old_chan_cap);
			name_delete(chan_cap_name_number);
		}
		result= name_append(chan_cap_name_number, &chan_cap);
		if (result != STD_OK)
		{
			fprintf(stderr, "%s: unable to append to '%s': %s\n",
				chan_cap_name_number, err_why(result));
			std_destroy(&chan_cap);
			exit(1);
		}
	}

#ifdef SUN4_TFTPBOOT_BUG
	result = open_channel(&eth_cap, &chan_cap1,
		     NWEO_SHARED | NWEO_DI_LOC | NWEO_DI_BROAD | NWEO_TYPESPEC,
		     ETH_ARP_PROTO, &ether);
	if (result != STD_OK) {
	    fprintf(stderr, "%s: fatal: open_channel (2) failed\n", prog_name);
	    exit(1);
	}

	result= tcpip_keepalive_cap(&chan_cap1);
	if (result != STD_OK)
	{
	    fprintf(stderr, "%s: fatal: tcpip_keepalive_cap failed (%s)\n",
		    prog_name, err_why(result));
	    exit(1);
	}
#endif

	if (threadno == 1) {
		my_ether_addr= ether;
		printf("%s: my ethernet address is %s\n",
		       prog_name, ether_ntoa(&my_ether_addr));
		if (rarp(&my_ether_addr, &my_ipaddr) != 0)
		{
			fprintf(stderr,
			     "%s: fatal: cannot find my own internet address\n",
			     prog_name);
			exit(1);
		}
	}

	for(;;)
	{
		mu_unlock(&thread_mutex);

		result= tcpip_read (&chan_cap, (char *)packet, sizeof(packet));

		mu_lock(&thread_mutex);

		if (result<0)
		{
			fprintf(stderr, "%s: fatal: tcpip_read failed (%s)\n",
				prog_name, err_why(result));
			exit(1);
		}

		if (result < sizeof(rarp46_t))
		{
			fprintf(stderr, "%s: packet too small\n", prog_name);
			continue;
		}
		rarp_ptr= (rarp46_t *)packet;
		if (rarp_ptr->a46_hdr != htons(RARP_ETHERNET))
		{
			fprintf(stderr, "%s: wrong hardware type\n", prog_name);
			continue;
		}
		if (rarp_ptr->a46_pro != htons(ETH_IP_PROTO))
		{
			fprintf(stderr, "%s: wrong protocol\n", prog_name);
			continue;
		}
		if (rarp_ptr->a46_hln != 6)
		{
			fprintf(stderr, "%s: wrong hardware address len (%d)\n",
						prog_name, rarp_ptr->a46_hln);
			continue;
		}
		if (rarp_ptr->a46_pln != 4)
		{
			fprintf(stderr, "%s: wrong protocol address len (%d)\n",
						prog_name, rarp_ptr->a46_pln);
			continue;
		}
		if (rarp_ptr->a46_op != htons(RARP_REQUEST))
		{
			if (rarp_ptr->a46_op != htons(ARP_REPLY)) {
				fprintf(stderr,"%s: wrong request %x from %x\n",
		    			prog_name, rarp_ptr->a46_op,
					rarp_ptr->a46_srcaddr);
			}
			continue;
		}
		result = 1;
		if (docache) {
			result = cache_rarp(&rarp_ptr->a46_tha,
					    (ipaddr_t *)rarp_ptr->a46_tpa);
		}
		if (result != 0) {
			result = rarp(&rarp_ptr->a46_tha,
				      (ipaddr_t *)rarp_ptr->a46_tpa);
		}
		if (result != 0)
			continue;
		memcpy(&reply_ipaddr, rarp_ptr->a46_tpa, sizeof(ipaddr_t));
		if ((reply_ipaddr & ip_netmask) != ip_net)
		{
			fprintf(stderr,
			"%s: (warning) IP address %s not on this network\n",
				prog_name, inet_ntoa(reply_ipaddr));
			continue;
		}
		rarp_ptr->a46_sha= my_ether_addr;
		(void) memcpy((_VOIDSTAR) rarp_ptr->a46_spa,
			      (_VOIDSTAR) &my_ipaddr, sizeof(ipaddr_t));
		rarp_ptr->a46_op= htons(RARP_REPLY);
		rarp_ptr->a46_dstaddr= rarp_ptr->a46_tha;
		result= tcpip_write (&chan_cap, (char *)packet,
			sizeof(rarp46_t));
		if (result<0)
		{
			fprintf(stderr, "%s: fatal: tcpip_write failed (%s)\n",
				prog_name, err_why(result));
			exit(1);
		}
#ifdef SUN4_TFTPBOOT_BUG
		/* send an extra arp reply, to introduce a cache entry */
		rarp_ptr->a46_sha= rarp_ptr->a46_tha;
		(void) memcpy((_VOIDSTAR) rarp_ptr->a46_spa,
			      (_VOIDSTAR) rarp_ptr->a46_tpa, sizeof(ipaddr_t));
		rarp_ptr->a46_tha= my_ether_addr;
		(void) memcpy((_VOIDSTAR) rarp_ptr->a46_tpa,
			      (_VOIDSTAR) &my_ipaddr, sizeof(ipaddr_t));
		rarp_ptr->a46_op= htons(ARP_REPLY);
		rarp_ptr->a46_dstaddr= rarp_ptr->a46_tha;
		result= tcpip_write (&chan_cap1, (char *)packet,
			sizeof(rarp46_t));
		if (result<0)
		{
			fprintf(stderr, "%s: fatal: tcpip_write failed (%s)\n",
				prog_name, err_why(result));
			exit(1);
		}
#endif
	}
}

int main(argc, argv)
int argc;
char *argv[];
{
	char *ip_cap_name;
	capability ip_cap, ip_chan_cap;
	nwio_ipconf_t ipconf;
	errstat result;
	int argindex;
	int i;

	prog_name= argv[0];
	mu_init(&print_mu);

	eth_cap_name= 0;
	chan_cap_name= 0;
	ip_cap_name= 0;
	for (argindex= 1; argindex<argc; argindex++)
	{
		if (!strcmp(argv[argindex], "-?"))
			usage();
		if (!strcmp(argv[argindex], "-v"))
		{
			verbose = 1;
			continue;
		}
		if (!strcmp(argv[argindex], "-E"))
		{
			argindex++;
			if (argindex >= argc)
				usage();
			if (eth_cap_name)
				usage();
			eth_cap_name= argv[argindex];
			continue;
		}
		if (!strcmp(argv[argindex], "-chan"))
		{
			argindex++;
			if (argindex >= argc)
				usage();
			if (chan_cap_name)
				usage();
			chan_cap_name= argv[argindex];
			continue;
		}
		if (!strcmp(argv[argindex], "-I"))
		{
			argindex++;
			if (argindex >= argc)
				usage();
			if (ip_cap_name)
				usage();
			ip_cap_name= argv[argindex];
			continue;
		}
		if (!strcmp(argv[argindex], "-cache"))
		{
			docache = 1;
			continue;
		}
		usage();
	}

	if (!eth_cap_name)
	{
		eth_cap_name= getenv("ETH_SERVER");
		if (eth_cap_name && !eth_cap_name[0])
			eth_cap_name =0;
	}
	if (!eth_cap_name)
		eth_cap_name= ETH_SVR_NAME;
	
	result= ip_host_lookup (eth_cap_name, "eth", &eth_cap);
	if (result != STD_OK)
	{
		fprintf(stderr, "%s: fatal: cannot lookup '%s' (%s)\n",
			prog_name, eth_cap_name, err_why(result));
		exit(1);
	}

	if (ip_cap_name)
	{
		/* Try to find out what network we are on. */
		if ((result= ip_host_lookup(ip_cap_name, "ip", &ip_cap)) != STD_OK)
		{
			fprintf(stderr, "%s: unable to lookup '%s': %s\n",
				prog_name, ip_cap_name, err_why(result));
			exit(1);
		}
		if ((result= tcpip_open(&ip_cap, &ip_chan_cap)) != STD_OK)
		{
			fprintf(stderr, "%s: unable to open IP channel: %s\n",
				prog_name, err_why(result));
			exit(1);
		}
		result= ip_ioc_getconf(&ip_chan_cap, &ipconf);
		std_destroy(&ip_chan_cap);
		if (result != STD_OK)
		{
			fprintf(stderr, "%s: unable to lookup IP address: %s\n",
				prog_name, err_why(result));
			exit(1);
		}
		ip_netmask= ipconf.nwic_netmask;
		ip_net= ipconf.nwic_ipaddr & ip_netmask;
	}
	else
	{
		/* Do not check replies. */
		ip_netmask= HTONL(0);
		ip_net= HTONL(0);
	}

	if (docache) {
	    /* preload the cache with all hosts in /etc/ethers */
	    if (cache_init() != 0) {
		fprintf(stderr, "%s: cache_init failed; caching disabled\n",
			prog_name);
		docache = 0;
	    }
	}

	for(i=1; i<NRARPTHREAD; i++)
		thread_newthread(svr_thread, 8192, (char *) 0, 0);
	svr_thread( (char *) 0, 0);
	/*NOTREACHED*/
}

static int rarp(ea, ia)
ether_addr_t *ea;
ipaddr_t *ia;
{
	char hostname[256];
	struct hostent *hostent;

	if (ether_ntohost(hostname, ea) != 0)
	{
		if (verbose)
		{
		    fprintf(stderr,
			"%s: unable to lookup name for ethernet address %s\n",
						    prog_name, ether_ntoa(ea));
		}
		return -1;
	}
	hostent= gethostbyname (hostname);
	if (!hostent)
	{
		if (verbose)
		{
		    fprintf(stderr, "%s: host '%s' not in host table\n",
							prog_name, hostname);
		}
		return -1;
	}
	if (hostent->h_addrtype != AF_INET)
	{
		if (verbose)
		{
		    fprintf(stderr, "%s: host '%s' has no internet addr\n",
							prog_name, hostname);
		}
		return -1;
	}
	(void) memcpy((_VOIDSTAR)ia, (_VOIDSTAR)hostent->h_addr, sizeof(*ia));
	return 0;
}

static void usage()
{
	fprintf(stderr,
"USAGE: %s [-cache] [-E <eth-cap>] [-I <ip-cap>] [-chan <chan-cap>] [-v]\n",
		prog_name);
	exit(1);
}

/*
 * Rarp Cache.
 *
 * Currently, no attempt is made to keep the cache consistent in case
 * the /etc/hosts or /etc/ethers files are changed while the rarp server
 * is running.  If this happens, make sure the server is restarted to
 * use the new information!  Note that plain host additions to the files are
 * harmless, because these will not be in the cache, and in that case the
 * default mechanism is used.
 */

#define MAXCACHE	256	/* should be enough for a while .. */

struct cache {
    char	*c_hostname;
    ether_addr_t c_etheraddr;
    ipaddr_t     c_ipaddr;
} Rarp_cache[MAXCACHE];

static int Rarp_cache_size = 0;

static int
cache_init()
{
    /* Load the cache with all hosts contained in /etc/ethers.
     * Note that we call rarp() for each host, which may consult the
     * Domain Name System instead of using /etc/hosts.
     * They should be consistent, however.
     */
    char	hostname[256];
    capability  ethers_file;
    b_fsize     size;
    b_fsize     nbytes;
    errstat     err;
    char *      buf;
    char *      bp;

    /* Get capability for ethers file  & bullet server */
    if ((err = name_lookup(ETHERS_FILE, &ethers_file)) != STD_OK)
    {
	fprintf(stderr, "%s: cannot lookup '%s': %s\n",
		prog_name, ETHERS_FILE, err_why(err));
	return 1;
    }

    /* Read the entire file into core - it should fit in memory */
    if ((err = b_size(&ethers_file, &size)) != STD_OK)
    {
	fprintf(stderr, "%s: cannot get size of '%s': %s\n",
		prog_name, ETHERS_FILE, err_why(err));
	return 1;
    }
    
    /*
    ** We allocate one byte extra for a null byte to terminate the file
    */
    if ((buf = (char *) malloc((size_t) size+1)) == 0)
	return 1;
    
    if ((err = b_read(&ethers_file, (b_fsize) 0, buf, size, &nbytes))!= STD_OK)
    {
	fprintf(stderr, "%s: Read of '%s' failed: %s\n",
		prog_name, ETHERS_FILE,err_why(err));
	free((_VOIDSTAR) buf);
	return 1;
    }

    if (size != nbytes) /* read didn't return all the data - don't trust it */
    {
	fprintf(stderr, "%s: did not get all of '%s'\n",prog_name, ETHERS_FILE);
	free((_VOIDSTAR) buf);
	return 1;
    }

    /*
     * Use function ether_line() from the library to scan the lines
     * from the ethers file.  In fact, the current function is a modified
     * version of ether_ntohost from the library.
     */
    *(buf+size) = '\0';
    bp = buf;
    while (*bp && Rarp_cache_size < MAXCACHE)
    {
	struct ether_addr eth_addr;
	ipaddr_t ip_addr;

        if (ether_line(bp, &eth_addr, hostname) == 0) {
	    if (rarp(&eth_addr, &ip_addr) == 0) {
		char *host;

		if (verbose) {
		    printf("%s: %8s: etheraddr = %s; ipaddr = %X\n", prog_name,
			   hostname, ether_ntoa(&eth_addr), ip_addr);
		}

		host = (char *) malloc(strlen(hostname) + 1);
		if (host == NULL) {
		    fprintf(stderr, "cache_in: malloc failed\n");
		    break;
		}
		strcpy(host, hostname);
		Rarp_cache[Rarp_cache_size].c_ipaddr = ip_addr;
		Rarp_cache[Rarp_cache_size].c_etheraddr = eth_addr;
		Rarp_cache[Rarp_cache_size].c_hostname = host;
		Rarp_cache_size++;
	    } else {
		if (verbose) {
		    printf("%s: %8s: etheraddr = %s; ipaddr **not found**\n",
			   prog_name, hostname, ether_ntoa(&eth_addr));
		}
	    }
        }

	/* skip to the start of the next line and look for more */
	while (*bp && *bp++ != '\n') {
	    /* do nothing */
	}
    }

    free((_VOIDSTAR) buf);
    if (Rarp_cache_size == 0) {
	printf("%s: no hosts found in %s\n", prog_name, ETHERS_FILE);
	return 1;
    } else {
	if (Rarp_cache_size == MAXCACHE) {
	    printf("%s: cache full\n", prog_name);
	}
	return 0;
    }
}


static int
cache_rarp(ea, ia)
ether_addr_t *ea;
ipaddr_t *ia; /* possibly unaligned! */
{
    int i;

    for (i = 0; i < Rarp_cache_size; i++) {
	struct cache *cache = &Rarp_cache[i];

	if (memcmp((_VOIDSTAR) ea, (_VOIDSTAR) &cache->c_etheraddr,
		   sizeof(*ea)) == 0)
	{
	    if (verbose) {
		mu_lock(&print_mu);
		printf("%s: return cached IP address %X for %s\n",
		       prog_name, cache->c_ipaddr, cache->c_hostname);
		mu_unlock(&print_mu);
	    }
	    (void) memcpy((_VOIDSTAR) ia, (_VOIDSTAR) &cache->c_ipaddr,
			  sizeof(*ia));
	    return 0;
	}
    }

    /* Not found in cache */
    if (verbose) {
	mu_lock(&print_mu);
	printf("%s: ipaddr for %s not in cache\n", prog_name, ether_ntoa(ea));
	mu_unlock(&print_mu);
    }
    return 1;
}
