/*	@(#)res_debug.c	1.1	91/11/19 09:58:05 */
/*-
 * Copyright (c) 1985, 1990 Regents of the University of California.
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
 *
 *	@(#)res_debug.c	5.30 (Berkeley) 6/27/90
 */

#define AMOEBA	1	/* Rediculous but nessesary */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)res_debug.c	5.30 (Berkeley) 6/27/90";
#endif /* LIBC_SCCS and not lint */

#if AMOEBA
#include <stdio.h>
#include <string.h>

#include <server/ip/hton.h>
#include <server/ip/types.h>

#include <server/ip/gen/in.h>
#include <server/ip/gen/inet.h>
#include <server/ip/gen/nameser.h>
#include <server/ip/gen/resolv.h>

typedef u16_t u_short;
typedef u32_t u_long;

static void fp_query _ARGS(( char *msg, FILE *file ));
static char *p_cdname _ARGS(( char *cp, char *msg, FILE *file ));
static char *p_type _ARGS(( int type ));
static char *p_class _ARGS(( int class ));
static char *p_rr _ARGS(( char *cp, char *msg, FILE *file ));
static char *p_time _ARGS(( u_long value ));

#define bcopy(s,d,l)	memcpy(d,s,l)
#else
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <arpa/nameser.h>

extern char *p_cdname(), *p_rr(), *p_type(), *p_class(), *p_time();
extern char *inet_ntoa();
#endif

char *_res_opcodes[] = {
	"QUERY",
	"IQUERY",
	"CQUERYM",
	"CQUERYU",
	"4",
	"5",
	"6",
	"7",
	"8",
	"UPDATEA",
	"UPDATED",
	"UPDATEDA",
	"UPDATEM",
	"UPDATEMA",
	"ZONEINIT",
	"ZONEREF",
};

char *_res_resultcodes[] = {
	"NOERROR",
	"FORMERR",
	"SERVFAIL",
	"NXDOMAIN",
	"NOTIMP",
	"REFUSED",
	"6",
	"7",
	"8",
	"9",
	"10",
	"11",
	"12",
	"13",
	"14",
	"NOCHANGE",
};

void
p_query(msg)
	char *msg;
{
	fp_query(msg,stdout);
}

/*
 * Print the contents of a query.
 * This is intended to be primarily a debugging routine.
 */
static void
fp_query(msg,file)
	char *msg;
	FILE *file;
{
	register char *cp;
	register dns_hdr_t *hp;
	register int n;

	/*
	 * Print header fields.
	 */
	hp = (dns_hdr_t *)msg;
	cp = msg + sizeof(dns_hdr_t);
	fprintf(file,"dns_hdr:\n");
	fprintf(file,"\topcode = %s", _res_opcodes[(hp->dh_flag1 & 
		DHF_OPCODE) >> 3]);
	fprintf(file,", id = %d", ntohs(hp->dh_id));
	fprintf(file,", rcode = %s\n", _res_resultcodes[hp->dh_flag2 & 
		DHF_RCODE]);
	fprintf(file,"\theader flags: ");
	if (hp->dh_flag1 & DHF_QR)
		fprintf(file," qr");
	if (hp->dh_flag1 & DHF_AA)
		fprintf(file," aa");
	if (hp->dh_flag1 & DHF_TC)
		fprintf(file," tc");
	if (hp->dh_flag1 & DHF_RD)
		fprintf(file," rd");
	if (hp->dh_flag2 & DHF_RA)
		fprintf(file," ra");
	if (hp->dh_flag2 & DHF_PR)
		fprintf(file," pr");
	fprintf(file,"\n\tqdcount = %d", ntohs(hp->dh_qdcount));
	fprintf(file,", ancount = %d", ntohs(hp->dh_ancount));
	fprintf(file,", nscount = %d", ntohs(hp->dh_nscount));
	fprintf(file,", arcount = %d\n\n", ntohs(hp->dh_arcount));
	/*
	 * Print question records.
	 */
	if (n = ntohs(hp->dh_qdcount)) {
		fprintf(file,"QUESTIONS:\n");
		while (--n >= 0) {
			fprintf(file,"\t");
			cp = p_cdname(cp, msg, file);
			if (cp == NULL)
				return;
			fprintf(file,", type = %s", 
				p_type(getshort((u8_t *)cp)));
			cp += sizeof(u_short);
			fprintf(file,", class = %s\n\n", 
				p_class(getshort((u8_t *)cp)));
			cp += sizeof(u_short);
		}
	}
	/*
	 * Print authoritative answer records
	 */
	if (n = ntohs(hp->dh_ancount)) {
		fprintf(file,"ANSWERS:\n");
		while (--n >= 0) {
			fprintf(file,"\t");
			cp = p_rr(cp, msg, file);
			if (cp == NULL)
				return;
		}
	}
	/*
	 * print name server records
	 */
	if (n = ntohs(hp->dh_nscount)) {
		fprintf(file,"NAME SERVERS:\n");
		while (--n >= 0) {
			fprintf(file,"\t");
			cp = p_rr(cp, msg, file);
			if (cp == NULL)
				return;
		}
	}
	/*
	 * print additional records
	 */
	if (n = ntohs(hp->dh_arcount)) {
		fprintf(file,"ADDITIONAL RECORDS:\n");
		while (--n >= 0) {
			fprintf(file,"\t");
			cp = p_rr(cp, msg, file);
			if (cp == NULL)
				return;
		}
	}
}

static char *
p_cdname(cp, msg, file)
	char *cp, *msg;
	FILE *file;
{
	char name[MAXDNAME];
	int n;

	if ((n = dn_expand((u8_t *)msg, (u8_t *)msg + 512, (u8_t *)cp, 
		(u8_t *)name, sizeof(name))) < 0)
		return (NULL);
	if (name[0] == '\0') {
		name[0] = '.';
		name[1] = '\0';
	}
	fputs(name, file);
	return (cp + n);
}

/*
 * Print resource record fields in human readable form.
 */
static char *
p_rr(cp, msg, file)
	char *cp, *msg;
	FILE *file;
{
	int type, class, dlen, n, c;
	char *cp1, *cp2;
	ipaddr_t inaddr;

	if ((cp = p_cdname(cp, msg, file)) == NULL)
		return (NULL);			/* compression error */
	fprintf(file,"\n\ttype = %s", p_type(type = getshort((u8_t *)cp)));
	cp += sizeof(u_short);
	fprintf(file,", class = %s", p_class(class = getshort((u8_t *)cp)));
	cp += sizeof(u_short);
	fprintf(file,", ttl = %s", p_time(getlong((u8_t *)cp)));
	cp += sizeof(u_long);
	fprintf(file,", dlen = %d\n", dlen = getshort((u8_t *)cp));
	cp += sizeof(u_short);
	cp1 = cp;
	/*
	 * Print type specific data, if appropriate
	 */
	switch (type) {
	case T_A:
		switch (class) {
		case C_IN:
		case C_HS:
			bcopy(cp, (char *)&inaddr, sizeof(inaddr));
			if (dlen == 4) {
				fprintf(file,"\tinternet address = %s\n",
					inet_ntoa(inaddr));
				cp += dlen;
			} else if (dlen == 7) {
				fprintf(file,"\tinternet address = %s",
					inet_ntoa(inaddr));
				fprintf(file,", protocol = %d", cp[4]);
				fprintf(file,", port = %d\n",
					(cp[5] << 8) + cp[6]);
				cp += dlen;
			}
			break;
		default:
			cp += dlen;
		}
		break;
	case T_CNAME:
	case T_MB:
	case T_MG:
	case T_MR:
	case T_NS:
	case T_PTR:
		fprintf(file,"\tdomain name = ");
		cp = p_cdname(cp, msg, file);
		fprintf(file,"\n");
		break;

	case T_HINFO:
		if (n = *cp++) {
			fprintf(file,"\tCPU=%.*s\n", n, cp);
			cp += n;
		}
		if (n = *cp++) {
			fprintf(file,"\tOS=%.*s\n", n, cp);
			cp += n;
		}
		break;

	case T_SOA:
		fprintf(file,"\torigin = ");
		cp = p_cdname(cp, msg, file);
		fprintf(file,"\n\tmail addr = ");
		cp = p_cdname(cp, msg, file);
		fprintf(file,"\n\tserial = %ld", getlong((u8_t *)cp));
		cp += sizeof(u_long);
		fprintf(file,"\n\trefresh = %s", p_time(getlong((u8_t *)cp)));
		cp += sizeof(u_long);
		fprintf(file,"\n\tretry = %s", p_time(getlong((u8_t *)cp)));
		cp += sizeof(u_long);
		fprintf(file,"\n\texpire = %s", p_time(getlong((u8_t *)cp)));
		cp += sizeof(u_long);
		fprintf(file,"\n\tmin = %s\n", p_time(getlong((u8_t *)cp)));
		cp += sizeof(u_long);
		break;

	case T_MX:
		fprintf(file,"\tpreference = %ld,",getshort((u8_t *)cp));
		cp += sizeof(u_short);
		fprintf(file," name = ");
		cp = p_cdname(cp, msg, file);
		break;

  	case T_TXT:
		(void) fputs("\t\"", file);
		cp2 = cp1 + dlen;
		while (cp < cp2) {
			if (n = (unsigned char) *cp++) {
				for (c = n; c > 0 && cp < cp2; c--)
					if (*cp == '\n') {
					    (void) putc('\\', file);
					    (void) putc(*cp++, file);
					} else
					    (void) putc(*cp++, file);
			}
		}
		(void) fputs("\"\n", file);
  		break;

	case T_MINFO:
		fprintf(file,"\trequests = ");
		cp = p_cdname(cp, msg, file);
		fprintf(file,"\n\terrors = ");
		cp = p_cdname(cp, msg, file);
		break;

	case T_UINFO:
		fprintf(file,"\t%s\n", cp);
		cp += dlen;
		break;

	case T_UID:
	case T_GID:
		if (dlen == 4) {
			fprintf(file,"\t%ld\n", getlong((u8_t *)cp));
			cp += sizeof(int);
		}
		break;

	case T_WKS:
		if (dlen < sizeof(u_long) + 1)
			break;
		bcopy(cp, (char *)&inaddr, sizeof(inaddr));
		cp += sizeof(u_long);
		fprintf(file,"\tinternet address = %s, protocol = %d\n\t",
			inet_ntoa(inaddr), *cp++);
		n = 0;
		while (cp < cp1 + dlen) {
			c = *cp++;
			do {
 				if (c & 0200)
					fprintf(file," %d", n);
 				c <<= 1;
			} while (++n & 07);
		}
		putc('\n',file);
		break;

#ifdef ALLOW_T_UNSPEC
	case T_UNSPEC:
		{
			int NumBytes = 8;
			char *DataPtr;
			int i;

			if (dlen < NumBytes) NumBytes = dlen;
			fprintf(file, "\tFirst %d bytes of hex data:",
				NumBytes);
			for (i = 0, DataPtr = cp; i < NumBytes; i++, DataPtr++)
				fprintf(file, " %x", *DataPtr);
			fputs("\n", file);
			cp += dlen;
		}
		break;
#endif /* ALLOW_T_UNSPEC */

	default:
		fprintf(file,"\t???\n");
		cp += dlen;
	}
	if (cp != cp1 + dlen) {
		fprintf(file,"packet size error (%#x != %#x)\n", cp, cp1+dlen);
		cp = NULL;
	}
	fprintf(file,"\n");
	return (cp);
}

static	char nbuf[40];

/*
 * Return a string for the type
 */
static char *
p_type(type)
	int type;
{
	switch (type) {
	case T_A:
		return("A");
	case T_NS:		/* authoritative server */
		return("NS");
	case T_CNAME:		/* canonical name */
		return("CNAME");
	case T_SOA:		/* start of authority zone */
		return("SOA");
	case T_MB:		/* mailbox domain name */
		return("MB");
	case T_MG:		/* mail group member */
		return("MG");
	case T_MR:		/* mail rename name */
		return("MR");
	case T_NULL:		/* null resource record */
		return("NULL");
	case T_WKS:		/* well known service */
		return("WKS");
	case T_PTR:		/* domain name pointer */
		return("PTR");
	case T_HINFO:		/* host information */
		return("HINFO");
	case T_MINFO:		/* mailbox information */
		return("MINFO");
	case T_MX:		/* mail routing info */
		return("MX");
	case T_TXT:		/* text */
		return("TXT");
	case T_AXFR:		/* zone transfer */
		return("AXFR");
	case T_MAILB:		/* mail box */
		return("MAILB");
	case T_MAILA:		/* mail address */
		return("MAILA");
	case T_ANY:		/* matches any type */
		return("ANY");
	case T_UINFO:
		return("UINFO");
	case T_UID:
		return("UID");
	case T_GID:
		return("GID");
#ifdef ALLOW_T_UNSPEC
	case T_UNSPEC:
		return("UNSPEC");
#endif /* ALLOW_T_UNSPEC */
	default:
		(void)sprintf(nbuf, "%d", type);
		return(nbuf);
	}
}

/*
 * Return a mnemonic for class
 */
static char *
p_class(class)
	int class;
{

	switch (class) {
	case C_IN:		/* internet class */
		return("IN");
	case C_HS:		/* hesiod class */
		return("HS");
	case C_ANY:		/* matches any class */
		return("ANY");
	default:
		(void)sprintf(nbuf, "%d", class);
		return(nbuf);
	}
}

/*
 * Return a mnemonic for a time to live
 */
static char *
p_time(value)
	u_long value;
{
	int secs, mins, hours;
	register char *p;

	if (value == 0) {
		strcpy(nbuf, "0 secs");
		return(nbuf);
	}

	secs = value % 60;
	value /= 60;
	mins = value % 60;
	value /= 60;
	hours = value % 24;
	value /= 24;

#define	PLURALIZE(x)	x, (x == 1) ? "" : "s"
	p = nbuf;
	if (value) {
		(void)sprintf(p, "%d day%s", PLURALIZE(value));
		while (*++p);
	}
	if (hours) {
		if (value)
			*p++ = ' ';
		(void)sprintf(p, "%d hour%s", PLURALIZE(hours));
		while (*++p);
	}
	if (mins) {
		if (value || hours)
			*p++ = ' ';
		(void)sprintf(p, "%d min%s", PLURALIZE(mins));
		while (*++p);
	}
	if (secs || ! (value || hours || mins)) {
		if (value || hours || mins)
			*p++ = ' ';
		(void)sprintf(p, "%d sec%s", PLURALIZE(secs));
	}
	return(nbuf);
}
