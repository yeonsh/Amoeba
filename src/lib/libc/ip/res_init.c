/*	@(#)res_init.c	1.2	96/02/27 11:09:01 */
/*-
 * Copyright (c) 1985, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define AMOEBA	1	/* Rediculous but nessesary */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)res_init.c	6.14 (Berkeley) 6/27/90";
#endif /* LIBC_SCCS and not lint */

#if AMOEBA
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <_ARGS.h>

#include <server/ip/hton.h>
#include <server/ip/types.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/inet.h>
#include <server/ip/gen/nameser.h>
#include <server/ip/gen/netdb.h>
#include <server/ip/gen/resolv.h>
#include <server/ip/gen/socket.h>

#define index(s,c) strchr(s,c)
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <arpa/nameser.h>
#include <resolv.h>
#endif

/*
 * Resolver state default settings
 */

struct state _res = {
	RES_TIMEOUT,               	/* retransmition time interval */
	4,                         	/* number of times to retransmit */
#ifdef DEBUG
	RES_DEFAULT | RES_DEBUG,	/* options flags */
#else
	RES_DEFAULT,			/* options flags */
#endif
	1,                         	/* number of name servers */
};

/*
 * Set up default settings.  If the configuration file exist, the values
 * there will have precedence.  Otherwise, the server address is set to
 * INADDR_ANY and the default domain name comes from the gethostname().
 *
 * The configuration file should only be used if you want to redefine your
 * domain or run without a server on your machine.
 *
 * Return 0 if completes successfully, -1 on error
 */
int
res_init()
{
	register FILE *fp;
	register char *cp, **pp;
	register int n;
	char buf[BUFSIZ];
	int nserv = 0;    /* number of nameserver records read from file */
	int haveenv = 0;
	int havesearch = 0;
	struct servent* servent;
	u16_t nameserver_port;

	servent= getservbyname("domain", NULL);
	if (!servent)
	{
		h_errno= NO_RECOVERY;
		return -1;
	}
	nameserver_port= servent->s_port;

	_res.nsaddr_list[0]= 0;
	_res.nsport_list[0]= nameserver_port;
	_res.nscount = 1;

	/* Allow user to override the local domain definition */
	if ((cp = getenv("LOCALDOMAIN")) != NULL) {
		(void)strncpy(_res.defdname, cp, sizeof(_res.defdname));
		haveenv++;
	}

	if ((fp = fopen(_PATH_RESCONF, "r")) != NULL) {
	    /* read the config file */
	    while (fgets(buf, sizeof(buf), fp) != NULL) {
		/* read default domain name */
		if (!strncmp(buf, "domain", sizeof("domain") - 1)) {
		    if (haveenv)	/* skip if have from environ */
			    continue;
		    cp = buf + sizeof("domain") - 1;
		    while (*cp == ' ' || *cp == '\t')
			    cp++;
		    if ((*cp == '\0') || (*cp == '\n'))
			    continue;
		    (void)strncpy(_res.defdname, cp, sizeof(_res.defdname) - 1);
		    if ((cp = index(_res.defdname, '\n')) != NULL)
			    *cp = '\0';
		    havesearch = 0;
		    continue;
		}
		/* set search list */
		if (!strncmp(buf, "search", sizeof("search") - 1)) {
		    if (haveenv)	/* skip if have from environ */
			    continue;
		    cp = buf + sizeof("search") - 1;
		    while (*cp == ' ' || *cp == '\t')
			    cp++;
		    if ((*cp == '\0') || (*cp == '\n'))
			    continue;
		    (void)strncpy(_res.defdname, cp, sizeof(_res.defdname) - 1);
		    if ((cp = index(_res.defdname, '\n')) != NULL)
			    *cp = '\0';
		    /*
		     * Set search list to be blank-separated strings
		     * on rest of line.
		     */
		    cp = _res.defdname;
		    pp = _res.dnsrch;
		    *pp++ = cp;
		    for (n = 0; *cp && pp < _res.dnsrch + MAXDNSRCH; cp++) {
			    if (*cp == ' ' || *cp == '\t') {
				    *cp = 0;
				    n = 1;
			    } else if (n) {
				    *pp++ = cp;
				    n = 0;
			    }
		    }
		    /* null terminate last domain if there are excess */
		    while (*cp != '\0' && *cp != ' ' && *cp != '\t')
			    cp++;
		    *cp = '\0';
		    *pp++ = 0;
		    havesearch = 1;
		    continue;
		}
		/* read nameservers to query */
		if (!strncmp(buf, "nameserver", sizeof("nameserver") - 1) &&
		   nserv < MAXNS) {
		    cp = buf + sizeof("nameserver") - 1;
		    while (*cp == ' ' || *cp == '\t')
			    cp++;
		    if ((*cp == '\0') || (*cp == '\n'))
			    continue;
		    if ((_res.nsaddr_list[nserv]=
			inet_addr(cp)) == (ipaddr_t)-1) {
			    _res.nsaddr_list[nserv]= 0;
			    continue;
		    }
		    _res.nsport_list[nserv]= nameserver_port;
		    nserv++;
		    continue;
		}
	    }
	    if (nserv > 1) 
		_res.nscount = nserv;
	    (void) fclose(fp);
	}
	if (_res.defdname[0] == 0) {
		if (gethostname(buf, sizeof(_res.defdname)) == 0 &&
		   (cp = index(buf, '.')))
			(void)strcpy(_res.defdname, cp + 1);
	}

	/* find components of local domain that might be searched */
	if (havesearch == 0) {
		pp = _res.dnsrch;
		*pp++ = _res.defdname;
		for (cp = _res.defdname, n = 0; *cp; cp++)
			if (*cp == '.')
				n++;
		cp = _res.defdname;
		for (; n >= LOCALDOMAINPARTS && pp < _res.dnsrch + MAXDFLSRCH;
		    n--) {
			cp = index(cp, '.');
			*pp++ = ++cp;
		}
		*pp++ = 0;
	}
	_res.options |= RES_INIT;
	return (0);
}
