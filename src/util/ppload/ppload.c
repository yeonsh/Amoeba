/*	@(#)ppload.c	1.4	96/02/27 13:11:26 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* ppload [-t timeout] [-v] [hostdir]
   Show load of all processors in a particular pool directory.
   Define UNIX to compile the Unix version; by the default the
   Amoeba version is compiled, which uses the 'jobset' package
   to poll several processors in parallel (and thus increase
   perceived speed when several processors are down).
*/

#include <amtools.h>
#include <stdlib.h>
#include <ampolicy.h>
#include <module/direct.h>
#include <module/name.h>
#include <module/proc.h>
#include <module/systask.h>

char *progname = "PPLOAD";

void fmtuptime();

#define K 1024

#ifdef UNIX
#define DEF_TOUT 2000L
#else
#define DEF_TOUT 5000L
#endif

void
usage()
{
	fprintf(stderr, "usage: %s [-t timeout] [-v] [hostdir]\n", progname);
	fprintf(stderr, "-t timeout: set time-out milliseconds [%ld]\n",
								DEF_TOUT);
	fprintf(stderr, "-v:         verbose (prints kernel versions)\n");
	fprintf(stderr, "hostdir:    hosts directory [%s]\n", HOST_DIR);
	exit(2);
}

int verbose;

char *pooldir;
capability pooldircap;
long tout = DEF_TOUT;

struct jobset *js_create();
long getuptime();

/*forward*/ void poll();

main(argc, argv)
	int argc;
	char **argv;
{
	errstat err;
	char *name;
	struct jobset *js;
	struct dir_open *dp;
	
	if (argc > 0 && argv[0] != NULL) {
		/* Try to set progname to basename(argv[0]) */
		char *p = strrchr(argv[0], '/');
		if (p == NULL)
			p = argv[0];
		else
			++p;
		if (*p != '\0')
			progname = p;
	}
	
	for (;;) {
		int c = getopt(argc, argv, "t:v");
		if (c == EOF)
			break;
		switch (c) {
		
		case 't':
			tout = atol(optarg);
			if (tout <= 0)
				usage();
			break;
		
		case 'v':
			verbose++;
			break;
		
		default:
			usage();
			break;
		
		}
	}
	
	if (optind + 1 < argc)
		usage();
	if (optind < argc)
		pooldir = argv[optind];
	else
		pooldir = HOST_DIR;
	
	if ((err = name_lookup(pooldir, &pooldircap)) != 0)
		fatal("can't find pooldir %s (%s)", pooldir, err_why(err));
	
	/* We don't care much for information about down processors --
	   so set a short locate timeout (subject to the -t option) */
	
	if ((dp = dir_open(&pooldircap)) == 0)
		fatal("can't open pooldir %s", pooldir);
	
#ifndef UNIX
	if ((js = js_create(16000, 4)) == NULL)
		fatal("can't create jobset");
#endif
	
	while ((name = dir_next(dp)) != NULL) {
		char *p;
		
		if (strchr(name, '.') != NULL)
			continue;
#ifndef UNIX
		if ((p = (char *) malloc(1 + strlen(name))) == NULL)
			fatal("insufficient memory");
		(void) strcpy(p, name);
		if (js_addjob(js, poll, p) < 0)
			fatal("can't add another job");
#else
		poll(name);
#endif
	}
	(void) dir_close(dp);
#ifndef UNIX
	js_destroy(js);
#endif
	exit(0);
}

void
getversion(cap, buf, buflen)
	capability *cap;
	char *buf;
	int buflen;
{
	errstat err;
	int n;
	
	err = sys_kstat(cap, 'v', buf, (bufsize) buflen, &n);
	if (ERR_STATUS(err)) {
		sprintf(buf, "(can't get version, sys_kstat: %s)",
								err_why(err));
		return;
	}
	else {
		if (buf[n-1] == '\n')
			--n;
		buf[n] = '\0';
	}
}

void
poll(name)
	char *name;
{
	capability hostcap, proccap, syscap;
	long load, ips, mem, uptime;
	errstat err;
	char buf[2000];
	static mutex mu;
	
	uptime = -1;
	(void) timeout(tout);
	if ((err = dir_lookup(&pooldircap, name, &hostcap)) != STD_OK) {
		mu_lock(&mu);
		printf("%-8s disappeared from directory (%s)\n",
			name, err_why(err));
		mu_unlock(&mu);
	}
	else if ((err = dir_lookup(&hostcap, PROCESS_SVR_NAME, &proccap))
								!= STD_OK) {
		mu_lock(&mu);
		printf("%-8s unreachable (proc lookup: %s)\n",
							name, err_why(err));
		mu_unlock(&mu);
	}
	else if ((err = pro_getload(&proccap, &ips, &load, &mem)) != STD_OK) {
		mu_lock(&mu);
		printf("%-8s no load information (getload: %s)\n",
							name, err_why(err));
		mu_unlock(&mu);
	}
	else if ((err = dir_lookup(&hostcap, DEF_SYSSVRNAME, &syscap))
								    != STD_OK) {
		mu_lock(&mu);
		printf("%-8s can't look up system server cap (%s)\n",
							name, err_why(err));
		mu_unlock(&mu);
	}
	else {
		/* Normalize load average. See comment in aps.c */
		load = (100*load + K/2)/K;
		sprintf(buf, "%-8s load %ld.%02.2ld, %ld.%ld Mips, %ld.%ld Mb free",
			name, load / 100, load % 100,
			ips/(K*1000), ((ips/K) % 1000) / 100,
			mem/(K*1000), ((mem/K) % 1000) / 100);
		uptime = getuptime(&syscap);
		if (uptime >= 0) {
			char *p = strchr(buf, '\0');
			(void) strcpy(p, ", up ");
			p = strchr(p, '\0');
			fmtuptime(p, uptime);
		}
		if (verbose) {
			char *p = strchr(buf, '\0');
			sprintf(p, "\n%-8s ", "");
			p = strchr(p, '\0');
			getversion(&syscap, p, (sizeof buf) - 2 - (p-buf));
		}
		mu_lock(&mu);
		printf("%s\n", buf);
		mu_unlock(&mu);
	}
#ifndef UNIX
	free(name);
#endif
}

/*VARARGS1*/
fatal(fmt, a1, a2, a3, a4)
	char *fmt;
{
	fprintf(stderr, "%s: ", progname);
	fprintf(stderr, fmt, a1, a2, a3, a4);
	fprintf(stderr, "\n");
	exit(1);
}
