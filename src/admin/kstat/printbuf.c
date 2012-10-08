/*	@(#)printbuf.c	1.5	94/04/06 11:47:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *	PRINTBUF
 *
 * This program sends a request to the specified amoeba kernel to return a
 * copy of the console buffer.
 *
 * Author:	Greg Sharp
 * Modified:	Greg Sharp, Sept 1991
 *		The systhread moved from being the kernel directory server to
 *		one level lower.  Thus we call syscap_lookup.
 *
 *		Philip Homburg, Oct 1991
 *		Added support for printbuf server
 */

#define _POSIX2_SOURCE	1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <amoeba.h>
#include <ampolicy.h>
#include <amtools.h>
#include <cmdreg.h>
#include <stderr.h>
#include <module/direct.h>
#include <module/host.h>
#include <module/systask.h>

#undef isprint
#define	isprint(c)	((c) >= ' ' && (c) <= '~')

#define PRINTBUF_SZ	30000
char	Reqbuf[PRINTBUF_SZ];
int	maxgarbage = 2;		/* number of nonprintables printed */
char *	prog_name;

void main _ARGS(( int argc, char *argv[] ));
static errstat printbuf_lookup _ARGS(( char *host, capability *cap ));
static void printchar _ARGS(( int c ));
static void usage _ARGS(( void ));

static errstat
printbuf_lookup(host, cap)
char *          host;
capability *    cap;
{
    errstat     err;
    capability  mcap;

    if ((err = host_lookup(host, &mcap)) != STD_OK)
	return err;
    err = dir_lookup(&mcap, DEF_PRINTBUFNAME, cap);
    return err;
}


void
main(argc, argv)
int	argc;
char *	argv[];
{

    register char *	end;
    errstat		error;
    capability		mcap;
    register char *	p;
    int			offset; /* where to start printing in the buffer */
    int			num_bytes; /* how many bytes returned in the buffer */
    int			c_flag;
    int			s_flag;
    int			p_flag;
    int 		c;
    char *		machine;

    prog_name= argv[0];

    c_flag = 0;
    s_flag = 0;
    p_flag = 0;

    while ((c= getopt(argc, argv, "?cps")) != -1)
    {
	switch(c)
	{
	case 'c':
		if (c_flag)
			usage();
		c_flag= 1;
		break;
	case 's':
		if (s_flag)
			usage();
		s_flag= 1;
		break;
	case 'p':
		if (p_flag)
			usage();
		p_flag= 1;
		break;
	case '?':
		usage();
	default:
		fprintf(stderr, "getopt failed\n");
		exit(1);
	}
    }

    if (optind >= argc)
	usage();
    machine= argv[optind];
    optind++;
    if (optind != argc)
	usage();

    if (p_flag)
	c_flag= 1;

    if (s_flag && c_flag)
	usage();

    for (;;sleep(60))
    {
	/* Look for new style printbufs */
	if (!s_flag)
	{
	    error = printbuf_lookup(machine, &mcap);
	    if (error != STD_OK && error != STD_NOTFOUND && 
		error != STD_COMBAD)
	    {
		fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
					prog_name, machine, err_why(error));
		if (p_flag)
			continue;
		exit(1);
	    }
	    if (error == STD_OK)
		c_flag= 1;
	}
	else
	    error = STD_NOTFOUND;	/* we didn't even look */

	/* Look for system server */
	if (error != STD_OK)
	{
	    error = syscap_lookup(machine, &mcap);
	    if (error != STD_OK)
	    {
		fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
					    prog_name, machine, err_why(error));
		if (p_flag)
			continue;
		exit(1);
	    }
	    if (!c_flag)
		s_flag= 1;
	}

	/* Get printbuffer */
	do
	{
	    error = sys_printbuf(&mcap, Reqbuf, PRINTBUF_SZ, &offset, 
								&num_bytes);
	    if (error != STD_OK)
	    {
		fprintf(stderr, "%s: sys_printbuf failed: %s\n", prog_name, 
								err_why(error));
		if (p_flag)
			break;
		exit(1);
	    }
	    /* check for sensible return values */
	    if (num_bytes <= 0 || num_bytes > PRINTBUF_SZ)
	    {
		fprintf(stderr,
		    "%s: returned num_bytes (0x%x) out of range (0 .. 0x%x]\n",
					    prog_name, num_bytes, PRINTBUF_SZ);
		if (p_flag)
			break;
		exit(1);
	    }
	    if (offset == num_bytes)
		offset = 0;
	    if (offset < 0 || offset >= num_bytes)
	    {
		fprintf(stderr,
			"%s: returned offset (0x%x) out of range [0 .. 0x%x)\n",
						prog_name, offset, num_bytes);
		if (p_flag)
			break;
		exit(1);
	    }
	    /*
	     * Then print the buffer, starting at offset, not forgetting to
	     * wrap around at the end!
	     */
	    end = Reqbuf + num_bytes;
	    p = Reqbuf + offset;
	    do
	    {
		printchar(*p++);
		if (p >= end)
		    p = Reqbuf;
	    } while (p != Reqbuf + offset);
	} while (c_flag);
	if (s_flag)
	    putchar('\n');
	if (!p_flag)
	    break;
    }
    exit(0);
}


/*
 * This routine avoids printing excess garbage characters in the printbuffer
 * which are still there from power-up time.
 */

static void
printchar(c)
int	c;
{
    static int garbage;

    if (isprint(c) && c != '\\' || c == '\n' || c == '\t')
    {
	if (garbage > maxgarbage)
	    printf(" etc...\n");
	garbage = 0;
	putchar(c);
    }
    else
    {
	if (++garbage > maxgarbage)
	    return;
	putchar('\\');
	switch (c)
	{
	    case '\0':      putchar('0');  break;
	    case '\b':      putchar('b');  break;
	    case '\r':      putchar('r');  break;
	    case '\f':      putchar('f');  break;
	    case '\\':      putchar('\\'); break;
	    default:        printf("%03o", c & 0xFF);
	}
    }
}

static void usage()
{
    fprintf(stderr, "USAGE: %s [-s] [-c] [-p] <machine>\n", prog_name);
    exit(1);
}
