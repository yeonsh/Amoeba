/*	@(#)tape_main.c	1.3	96/02/27 13:17:38 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** tape.c, a program which allows you to manage a tape with just one
** command, namely tape.
**
** mt parses the parameters and does the specified tape action, e.g.:
** 	tape [-f tape_cap] command
**
** command specifies the subcommand, e.g.:
**	write -c10 -r10240 -v
**
** peterb@cwi.nl, 120190
**
*/
#include <stdio.h>
#include <amtools.h>
#include <ampolicy.h>
#include <module/tape.h>
#include "commands.h"

/*
** externals...
**
*/
extern int optind;
extern char *optarg;
extern void mtape_read(), mtape_write();
extern void mtape_unload(), mtape_erase();
extern void mtape_fskip(), mtape_rskip();
extern void mtape_load(), mtape_rewind();
extern void mtape_status();

/*
** global variables.
**
*/
static char *mtape_cap = DEF_TAPESVR;

void (*tape_func[])() = {
	mtape_read,
	mtape_write,
	mtape_unload,
	mtape_erase,
	mtape_fskip,
	mtape_rskip,
	mtape_load,
	mtape_rewind,
	mtape_status,
	};

char *commands[] = {
	"read",			/* READ */
	"write",		/* WRITE */
	"unload",		/* UNLOAD */
	"erase",		/* ERASE */
	"fskip",		/* FSKIP */
	"rskip",		/* RSKIP */
	"load",			/* LOAD	 */
	"rewind",		/* REWIND */
	"status",		/* STATUS */
	};

static char *options[] = {
	"?c:r:vb:",	/* READ: record_count, record_size, verbose, buffers */	
	"?c:r:vb:",	/* WRITE: record_count, record_size, verbose, buffers */
	"?",		/* UNLOAD: no options */
	"?",		/* ERASE: no options */
	"?c:",		/* FSKIP: file_skip_count */
	"?c:",		/* RSKIP: record_skip_count */
	"?",		/* LOAD: no options */
	"?w",		/* REWIND: wait_for_completion */
	"?",		/* STATUS: no options */
	};

long	count = -1;
int	record_size = 512;
int32	skip_count = 1;
int	immediate = 1;
int	verbose;
long	buffer_count = MAX_NUM_BUFS;
long	in, out;
errstat	err;
bufsize	actual;
uint8	trans_buf[T_REQBUFSZ];
char   *cmd;

char *usage[] = {
	"[-r record_size] [-c count] [-v] [-b buffer_count]",	/* READ */
	"[-r record_size] [-c count] [-v] [-b buffer_count]",	/* WRITE */
	"",					/* UNLOAD */
	"",					/* ERASE */
	"[-c file_skip_count]",			/* FSKIP */
	"[-c record_skip_count]",		/* RSKIP */
	"",					/* LOAD */
	"[-w]",					/* REWIND */
	"",					/* STATUS */
	};


capability tape_cap;

/*
** functions.
**
*/
long_usage()
{
	int n;

	fprintf(stderr, "Usage: %s [-f tape_cap] subcommand\n", cmd);
	fprintf(stderr, "     : subcommands\n");
	for (n = 0; n != sizeof(usage)/sizeof(usage[0]); n++)
		fprintf(stderr, "     :             : %s %s\n", 
			commands[n], usage[n]);
	fprintf(stderr, "     : defaults\n");
	fprintf(stderr, "     :             : tape_cap %s\n", mtape_cap);
	fprintf(stderr, "     :             : record_size %d\n", record_size);
	if (count < 0)
		fprintf(stderr, "     :             : count infinite\n");
	else
		fprintf(stderr, "     :             : count %d\n", count);
	fprintf(stderr, "     :             : verbose %s\n", 
		(verbose == 0)? "off": "on");
	fprintf(stderr, "     :             : buffer_count %d\n", buffer_count);
	fprintf(stderr, "     :             : file_skip_count %d\n", 
		skip_count);
	fprintf(stderr, "     :             : record_skip_count %d\n", 
		skip_count);
	fprintf(stderr, "     :             : immediate %s\n", 
		(immediate == 0)? "off": "on");
	exit(1);
}

command_usage(n)
	int n;
{
	fprintf(stderr, "Usage: %s [-f tape_cap] %s %s\n",
		cmd, commands[n], usage[n]);
	exit(1);
}


main(argc, argv)
	char **argv;
{
	int opt, tape_unit_arg = 0;
	char *tmp_tape;

	cmd = argv[0];
	while ((opt = getopt(argc, argv, "f:")) != EOF)
	switch (opt) {
	case 'f':	mtape_cap = optarg;
			tape_unit_arg = 1;
			break;
	default:	long_usage();
			break;
	}

	if (optind == argc)
		long_usage();

	if (tape_unit_arg == 0 && 
	   (tmp_tape = getenv("TAPE")) && 
	   *tmp_tape != '\0')
		mtape_cap = tmp_tape;

        if (name_lookup(mtape_cap, &tape_cap) < 0) {
		fprintf(stderr, "%s: can't get capability for %s\n",
				cmd, mtape_cap);
		exit(1);
	}

	action(argc - optind, &argv[optind]);
	exit(0);
}

int
action(argc, argv)
	char **argv;
{
	int n, opt;

	for (n = 0; n != sizeof(commands)/sizeof(commands[0]); n++)
		if (!strcmp(argv[0], commands[n]))
			break;
	if (n == sizeof(commands)/sizeof(commands[0])) {
		long_usage();
		/*NOTREACHED*/
	}

	optind = 1;
	while ((opt = getopt(argc, argv, options[n])) != EOF)
	switch (opt) {
	case 'c':	skip_count = count = strtol(optarg, (char **) 0, 0);
			break;
	case 'r':	record_size = strtol(optarg, (char **) 0, 0);
			break;
	case 'b':	buffer_count = strtol(optarg, (char **) 0, 0);
			break;
	case 'w':	immediate = 0;
			break;
	case 'v':	verbose = 1;
			break;
	default:	command_usage(n);
			/*NOTREACHED*/
	}
	if (optind != argc) {
		command_usage(n);
		/*NOTREACHED*/
	}

	(tape_func[n])();
}

write_verbose()
{
	fprintf(stderr, "%d records in.\n%d records out.\n", in, out);
}
