/*	@(#)stun.c	1.7	96/02/27 13:07:44 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* stun [-s sig] [-p] [-c] [-v] process ...
   Stun one or more processes, optionally print checkpoints. */

#include "amtools.h"
#include "module/proc.h"
#include "exception.h"
#include "module/rnd.h"
#include "module/host.h"

#define DEFSIG 3 /* Default signal (corresponds to SIGQUIT) */


char *progname = "STUN";
char *dumpfile = NULL;

void
usage()
{
	fprintf(stderr,
"usage: %s [-d corefile] [-m machine] [-s sig] [-p] [-c] [-v] process ...\n",
								progname);
	fprintf(stderr, "-d file:    dump process to file (implies -p)\n");
	fprintf(stderr, "-m machine: interpret processes in machine/ps\n");
	fprintf(stderr, "-s sig:     stun code [%d]\n", DEFSIG);
	fprintf(stderr, "-p:         print checkpoint here\n");
	fprintf(stderr,
		"-c:         continue process afterwards (implies -p)\n");
	fprintf(stderr, "-v:         verbose\n");
	exit(2);
}

int verbose;
int print_dump;
int continue_proc;
signum signo = DEFSIG;
capability root;
int use_root;
port svr_port;
capability new_owner;
char *pdbuf;

errstat kill_proc(); /* Forward */

main(argc, argv)
	int argc;
	char **argv;
{
	int i;
	int sts;
	
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
		int c = getopt(argc, argv, "d:m:s:vcp");
		if (c == EOF)
			break;
		switch (c) {
		
		case 'd':
			dumpfile = optarg;
			print_dump++;
			break;
		
		case 'm':
			set_machine(optarg);
			break;
		
		case 's':
			signo = atol(optarg);
			break;
		
		case 'v':
			verbose++;
			break;
		
		case 'c':
			continue_proc++;
			print_dump++;
			break;
		
		case 'p':
			print_dump++;
			break;
		
		default:
			usage();
			break;
		
		}
	}
	
	if (optind >= argc)
		usage();
	
	if (print_dump) {
		uniqport(&svr_port);
		priv2pub(&svr_port, &new_owner.cap_port);
	}
	
	sts = 0;
	for (i = optind; i < argc; ++i) {
		if (kill_proc(argv[i]) != STD_OK)
			sts = 1;
	}
	
	exit(sts);
}

set_machine(name)
	char *name;
{
	errstat err;
	capability host;
	
	if ((err = host_lookup(name, &host)) != STD_OK) {
		fprintf(stderr, "%s: can't find machine %s (%s)\n",
			progname, name, err_why(err));
		exit(1);
	}
	if ((err = dir_lookup(&host, PROCESS_LIST_NAME, &root)) != STD_OK) {
		fprintf(stderr, "%s: can't find %s/%s (%s)\n",
			progname, name, PROCESS_LIST_NAME, err_why(err));
		exit(1);
	}
	use_root++;
}

errstat
kill_proc(name)
	char *name;
{
	errstat dump_proc();
	
	capability proc;
	capability owner;
	errstat err;
	
	if (use_root)
		err = dir_lookup(&root, name, &proc);
	else
		err = name_lookup(name, &proc);
	if (err != STD_OK) {
		fprintf(stderr, "%s: can't find %s (%s)\n",
			progname, name, err_why(err));
		return err;
	}
	
	if (print_dump) {
		if (verbose)
			printf("Getting owner for %s\n", name);
		if ((err = pro_getowner(&proc, &owner)) != STD_OK) {
			fprintf(stderr, "%s: can't get owner of %s (%s)\n",
				progname, name, err_why(err));
			return err;
		}
		if (verbose)
			printf("Changing owner for %s\n", name);
		if ((err = pro_setowner(&proc, &new_owner)) != STD_OK) {
			fprintf(stderr, "%s: can't change owner of %s (%s)\n",
				progname, name, err_why(err));
			return err;
		}
		if (verbose)
			printf("Owner changed for %s.\n", name);
	}
	
	if (verbose)
		printf("Stunning %s with %d...\n", name, signo);
	if ((err = pro_stun(&proc, signo)) != STD_OK) {
		fprintf(stderr, "%s: can't stun %s (%s)%s\n",
			progname, name, err_why(err),
			print_dump ? " (owner not changed back!)" : "");
		return err;
	}
	if (verbose)
		printf("Stunned %s.\n", name);
	
	if (print_dump)
		return dump_proc(name, &proc, &owner);
	else
		return STD_OK;
}

errstat
dump_proc(name, proc, owner)
	char *name;
	capability *proc;
	capability *owner;
{
	bufsize do_checkpoint();

	errstat err;
	header h;
	bufsize n;
	int done;
	
	if (verbose)
		printf("Awaiting checkpoint for %s\n", name);
	
	/* allocate a buffer to receive a process descriptor if we
	** haven't done it yet
	*/
	if (pdbuf == 0)
	    if ((pdbuf = malloc(MAX_PDSIZE)) == 0)
	    {
		printf("%s: dump_proc: cannot malloc buffer\n", progname);
		exit(1);
	    }

	for ( done = 0; !done; ) {
		h.h_port = svr_port;
		n = getreq(&h, pdbuf, MAX_PDSIZE);
		if (ERR_STATUS(n)) {
			err = ERR_CONVERT(n);
			fprintf(stderr, "%s: getreq for %s failed (%s)\n",
				progname, name, err_why(err));
			(void) pro_setowner(proc, owner);
			return err;
		}
		
		switch (h.h_command) {
		
		case STD_INFO:
			sprintf(pdbuf, "stun awaiting checkpoint for %s",
									name);
			h.h_size = n = strlen(pdbuf);
			h.h_status = STD_OK;
			break;
		
		default:
			if (verbose)
				fprintf(stderr,
					"%s: unexpected command %d for %s\n",
					progname, h.h_command, name);
			n = 0;
			h.h_status = STD_COMBAD;
			break;
		
		case PS_CHECKPOINT:
			n = do_checkpoint(name, proc, owner, &h, n);
			done = 1;
		}
		putrep(&h, pdbuf, n);
	}
	
	return STD_OK;
}

bufsize
do_checkpoint(name, proc, owner, h, n)
	char *name;
	capability *proc;
	capability *owner;
	header *h;
	bufsize n;
{
	errstat err;
	
	if (verbose)
		printf("Got checkpoint for %s\n", name);
	
	(void) handle((int) h->h_extra, h->h_offset, pdbuf, (int) n);
	
	if (verbose)
		printf("Restoring owner for %s\n", name);
	err = pro_setowner(proc, owner);
	if (err != STD_OK)
		fprintf(stderr, "%s: can't restore owner for %s (%s)\n",
			progname, name, err_why(err));
	
	if (continue_proc && err == STD_OK) {
		if (verbose)
			printf("Won't notify owner for %s\n", name);
	}
	else if (NULLPORT(&owner->cap_port)) {
		n = 0;
		if (verbose)
			printf("No owner for %s\n", name);
	}
	else {
		if (verbose)
			printf("Notifying owner for %s\n", name);
		h->h_port = owner->cap_port;
		h->h_priv = owner->cap_priv;
		n = trans(h, pdbuf, n, h, pdbuf, MAX_PDSIZE);
		if (ERR_STATUS(n)) {
			err = ERR_CONVERT(n);
			fprintf(stderr,
				"%s: can't notify owner for %s (%s)\n",
				progname, name, err_why(err));
		}
		else {
			err = h->h_status;
			if (err != STD_OK)
				fprintf(stderr,
					"%s: %s abandoned by owner (%s)\n",
					progname, name, err_why(err));
		}
	}
	
	if (verbose)
		if (err == STD_OK && n != 0)
			printf("Continuing %s\n", name);
		else
			printf("Killing %s\n", name);
	
	h->h_status = err;
	return n;
}

#ifdef DEBUG
dumpcap(str, cap)
	char *str;
	capability *cap;
{
	int i;
	
	printf("%s: ", str);
	for (i = 0; i < CAPSIZE; ++i)
		printf("%02x.", ((unsigned char *)cap)[i]);
	printf("\n");
	fflush(stdout);
}
#endif
