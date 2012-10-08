/*	@(#)monitor.c	1.5	96/02/27 13:09:52 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define MONWHIZ

#include "amoeba.h"
#include "monitor.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "module/name.h"

header	hdr;
MN_b_t	mondata;
int	monwrongendian;
bufsize	monsize;
char *	progname;

unsigned short cs(s)
unsigned short s;
{

	if (monwrongendian)
		return (s>>8)|(s<<8);
	else
		return s;
}

unsigned long cl(l)
unsigned long l;
{

	if (monwrongendian)
		return cs(l>>16)|(cs(l)<<16);
	else
		return l;
}

errstat
montrans(cmd, arg)
int	cmd;
int	arg;
{
	errstat	err;

	hdr.h_command = STD_MONITOR;
	hdr.MN_command = cmd;
	hdr.MN_arg = arg;
	if ((err = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0)) != 0) {
		fprintf(stderr, "%s: trans to monitored server failed: %s\n",
							progname, err_why(err));
		return ERR_CONVERT(err);
	}
	return ERR_CONVERT(hdr.h_status);
}

getdata() 
{
	register MN_ev_p mp;

	hdr.h_command = STD_MONITOR;
	hdr.MN_command = MNC_ENQUIRY;
	monsize = trans(&hdr, NILBUF, 0, &hdr, mondata.MN_cbuf, MAXMONSIZE);
	if (ERR_STATUS(monsize) || hdr.h_status != MNS_OK) {
		fprintf(stderr, "%s: Couldn't get monitor data: %s\n",
			progname, err_why((errstat) (ERR_STATUS(monsize) ?
			    ERR_CONVERT(monsize) : ERR_CONVERT(hdr.h_status))));
		exit(-1);
	}
	for (mp = mondata.MN_mbuf; mp->MN_soff; mp++)
		if (mp->MN_soff < 0 || (bufsize) mp->MN_soff >= monsize)
			monwrongendian++;
}

printstats(dototal, docircbuf)
int	dototal;
int	docircbuf;
{
	register MN_ev_p mp;
	
	if (dototal) {
		for (mp = mondata.MN_mbuf; mp->MN_soff; mp++)
			printf("%8ld   %s\n", cl(mp->MN_cnt),
					mondata.MN_cbuf+cs(mp->MN_soff));
		if (mp->MN_cnt != 0)
			printf("Server had monitor buffer overflow\n");
	}
	if (docircbuf) {
		register short *circbase;
		register circsize,i;

		printf("Last events:\n");
		circsize = cs(mondata.MN_sbuf[monsize/2-1]);
		circbase = &mondata.MN_sbuf[monsize/2-1-circsize];
		i = cs(*circbase);
		do {
			if (circbase[i])
				printf("\t%s\n", mondata.MN_cbuf +
							cs(circbase[i]));
			i++;
			if (i >= circsize)
				i=1;
		} while (i != cs(*circbase));
	}
}

errstat
do_circbuf(argc, argv)
int	argc;
char **	argv;
{
    if (argc == 1) {
	    getdata();
	    printstats(0, 1);
	    return STD_OK;
    } else if (strcmp(argv[1], "on") == 0)
	    return montrans(MNC_SETAFLAGS, MNF_CIRBUF);
    else if (strcmp(argv[1], "off") == 0)
	    return montrans(MNC_SETAFLAGS, 0);
    else {
	    fprintf(stderr, "Usage: %s port circbuf [on|off]\n", progname);
	    return STD_ARGBAD;
    }
}

/*ARGSUSED*/
errstat
do_reset(argc, argv)
int	argc;
char ** argv;
{
    return montrans(MNC_RESET, 0);
}

errstat
do_raw(argc, argv)
int argc;
char **argv;
{

	getdata();
	write(1, mondata, monsize);
	return STD_OK;
}

struct switchtab {
	char *	cmd;
	errstat (*rtn)();
} switchtab[] = {
	"circbuf",	do_circbuf,
	"reset",	do_reset,
	"raw",		do_raw,
	0,		0
};

main(argc, argv)
int	argc;
char *	argv[];
{
    capability	cap;
    errstat	err;

    progname = argv[0];
    if (argc < 2) {
	    fprintf(stderr, "Usage: %s port [cmd [args]]\n", progname);
	    exit(-1);
    }
    if ((err = name_lookup(argv[1], &cap)) != STD_OK) {
	    fprintf(stderr, "%s: directory lookup of %s failed: %s\n",
					    progname, argv[1], err_why(err));
	    exit(-1);
    }
    hdr.h_port = cap.cap_port;
    if (argc > 2) {
	    register struct switchtab *sp;

	    for(sp = switchtab; sp->cmd; sp++) {
		    if (strcmp(sp->cmd, argv[2]) == 0)
			    if ((*sp->rtn)(argc-2, argv+2) != STD_OK)
				exit(-1);
			    else
				exit(0);
	    }
	    fprintf(stderr, "%s: cmd's recognized:", progname);
	    for(sp = switchtab; sp->cmd; sp++)
		fprintf(stderr, " %s", sp->cmd);
	    fprintf(stderr, "\n");
	    exit(-1);
    }
    getdata();
    printstats(1, 0);
    exit(0);
}
