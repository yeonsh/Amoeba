/*	@(#)flcontrol.c	1.4	94/04/06 11:45:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * flip_ctrl
 *	Probably written by Philip Homburg and/or Frans Kaashoek
 *	Modified: Gregory J. Sharp, Oct 1993 -	merged with version that does
 *						the unix kernel.
 */

#define _POSIX2_SOURCE 1

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <amtools.h>
#include <module/buffers.h>
#include <module/host.h>
#include <server/systask/systask.h>

char *prog_name;
char reqbuf[16];
char replbuf[12];

static void usage _ARGS(( void ));
void main _ARGS(( int argc, char *argv[] ));

static void
usage()
{
    char * options = "[-d <delay>] [-l <loss>] [-o <on>] [-t <trusted>]";

    fprintf(stderr, 
	"USAGE: For Amoeba host:\n\t%s %s machine ntw\n", prog_name, options);
#ifndef AMOEBA
    fprintf(stderr, "USAGE: For Un*x host:\n\t%s %s ntw\n", prog_name, options);
#endif
    exit(1);
}

void
main(argc, argv)
	int argc;
	char *argv[];
{
	int c;
	int16 delay, loss, nw, on, trusted;
	char *d_arg= NULL, *l_arg= NULL, *o_arg= NULL, *nw_arg= NULL, *t_arg=
	    NULL;
	char *check, *bp, *ep;
	char *host_name;
	capability cap;
	bufsize size;
	errstat err;
	header hdr;

	prog_name= argv[0];

	while ((c= getopt(argc, argv, "d:l:o:t:")) != -1)
	{
		switch(c)
		{
		case 'd':
			if (d_arg)
				usage();
			d_arg= optarg;
			break;
		case 'l':
			if (l_arg)
				usage();
			l_arg= optarg;
			break;
		case 'o':
			if (o_arg)
				usage();
			o_arg= optarg;
			break;
		case 't':
			if (t_arg)
				usage();
			t_arg= optarg;
			break;
		default:
			usage();
			break;
		}
	}

	if (optind >= argc)
		usage();

#ifndef AMOEBA
	/* Under UNIX we may be updating the Unix kernel or an Amoeba host
	 * depending on the number of arguments.
	 */
	if (argc == optind + 2)
	{
		host_name= argv[optind];
		optind++;
	}
	else
		host_name = (char *) 0;
#else
	host_name= argv[optind];
	optind++;
#endif

	if (optind >= argc)
		usage();

	nw_arg= argv[optind];
	optind++;

	if (optind != argc)
		usage();

	if (d_arg)
	{
		delay= strtol(d_arg, &check, 0);
		if (check[0] || delay < 0)
		{
			fprintf(stderr, "%s: invalid value: '%s'\n", prog_name,
				d_arg);
			usage();
		}
	}
	else
		delay= -1;

	if (l_arg)
	{
		loss= strtol(l_arg, &check, 0);
		if (check[0] || loss < 0)
		{
			fprintf(stderr, "%s: invalid value: '%s'\n", prog_name, 
				l_arg);
			usage();
		}
	}
	else
		loss= -1;

	if (o_arg)
	{
		on= strtol(o_arg, &check, 0);
		if (check[0] || on < 0)
		{
			fprintf(stderr, "%s: invalid value: '%s'\n", prog_name, 
				o_arg);
			usage();
		}
	}
	else
		on= -1;

	if (t_arg)
	{
		trusted= strtol(t_arg, &check, 0);
		if (check[0] || trusted < 0)
		{
			fprintf(stderr, "%s: invalid value: '%s'\n", prog_name, 
				t_arg);
			usage();
		}
	}
	else
		trusted= -1;

	nw= strtol(nw_arg, &check, 0);
	if (check[0])
	{
		fprintf(stderr, "%s: invalid value: '%s'\n", prog_name, nw_arg);
		usage();
	}

#ifndef AMOEBA
	if (host_name == 0)
	{
		printf("%s: updating unix kernel\n", prog_name);
		if (flctrl_control(nw, &delay, &loss, &on, &trusted) < 0)
		{
			fprintf(stderr, "flip_control failed\n");
			exit(1);
		}
	}
	else
#endif
	{
	    err = syscap_lookup(host_name, &cap);
	    if (err != STD_OK)
	    {
		    fprintf(stderr, "%s: syscap_lookup of '%s' failed: %s\n",
			    prog_name, host_name, err_why(err));
		    exit(1);
	    }

	    hdr.h_port= cap.cap_port;
	    hdr.h_priv= cap.cap_priv;
	    hdr.h_command= SYS_NETCONTROL;
	    bp= reqbuf;
	    ep= reqbuf+sizeof(reqbuf);
	    bp= buf_put_int16(bp, ep, nw);
	    bp= buf_put_int16(bp, ep, delay);
	    bp= buf_put_int16(bp, ep, loss);
	    bp= buf_put_int16(bp, ep, on);
	    bp= buf_put_int16(bp, ep, trusted);
	    assert (bp);
	    size = trans(&hdr, reqbuf, (bufsize) (bp - reqbuf),
			 &hdr, replbuf, (bufsize) sizeof(replbuf));
	    if (ERR_STATUS(size)) {
		    err = ERR_CONVERT(size);
	    } else {
		    err = ERR_CONVERT(hdr.h_status);
	    }
	    if (err != STD_OK)
	    {
		    fprintf(stderr, "%s: netcontrol failed (%s)\n",
			    prog_name, err_why(err));
		    exit(1);
	    }
	    bp= replbuf;
	    ep= replbuf + size;
	    bp= buf_get_int16(bp, ep, &delay);
	    bp= buf_get_int16(bp, ep, &loss);
	    bp= buf_get_int16(bp, ep, &on);
	    bp= buf_get_int16(bp, ep, &trusted);

	    if (bp != ep)
	    {
		    fprintf(stderr, "%s: invalid buffer replied\n", prog_name); 
		    exit(1);
	    }
	}

	if (on)
	{
		printf("%s: flip network %d is on with delay %dms, loss %d%%, trusted %d\n",
		       prog_name, nw, delay, loss, trusted);
	}
	else
		printf("%s: flip network %d is off\n", prog_name, nw);
	exit(0);
}
