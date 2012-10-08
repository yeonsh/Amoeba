/*	@(#)sp_fix.c	1.4	96/02/27 10:21:13 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <amtools.h>
#include <capset.h>
#include <soap/soap.h>
#include <soap/super.h>	/* For BLOCKSIZE, L2VBLKSZ and PAIRSPERBLOCK */
#include <capset.h>
#include <module/disk.h>
#include <module/name.h>
#include <module/ar.h>


capability nullcap;
int get_flag, delete_flag;

main(argc, argv)
	int argc;
	char **argv;
{
	int         dirnum;
	disk_addr   blocknum;
	errstat     err;
	capability  supercap;
	char        buf[512];
	char       *cp_cap0, *cp_cap1;
	capability *cap[2];
	capset      cs;
	char        answer[100];

	while (argc > 1 && argv[1][0] == '-') {
		switch (argv[1][1]) {
		    case 'g': ++get_flag;
			break;
		    case 'd': ++delete_flag;
			break;
		    default:
			argc = 1;	/* Cause Usage error, below */
		}
		/* Shift the arg list, preserving argv[0]: */
		argv[1] = argv[0];
		argv++;
		argc--;
	}
	if (delete_flag && get_flag) {
		fprintf(stderr,
			"Cannot specify both get (-g) and delete (-d)\n");
		argc = 1;	/* Cause Usage error, below */
	}

	if (argc != 3 || (dirnum = atoi(argv[2])) <= 0) {
		fprintf(stderr,
		    "Usage: %s [-d | -g] SOAP-disk-partion dir-num\n", argv[0]);
		exit(2);
	}

	if ((err = name_lookup(argv[1], &supercap)) != STD_OK) {
		fprintf(stderr, "Cannot lookup %s (%s)\n",
					argv[1], err_why(err));
		exit(1);
	}

	blocknum = 1 + dirnum / PAIRSPERBLOCK;
	if ((err = disk_read(&supercap, L2VBLKSZ, blocknum, (disk_addr) 1, buf))
								!= STD_OK) {
		fprintf(stderr, "Cannot read block %ld (%s)\n",
					blocknum, err_why(err));
		exit(1);
	}

	cp_cap0 = buf + (dirnum % PAIRSPERBLOCK) * 2 * sizeof(capability);
	cp_cap1 = cp_cap0 + sizeof(capability);
	cap[0] = (capability *)cp_cap0;
	cap[1] = (capability *)cp_cap1;

	fprintf(stderr, "Current caps:\n");
	fprintf(stderr, "\tcap 0 = %s\n", ar_cap(cap[0]));
	fprintf(stderr, "\tcap 1 = %s\n", ar_cap(cap[1]));

	if (get_flag) {
		int i, num_caps = 0, cs_status;
		char *p;
		for (i = 0; i < 2; i++) {
			if (NULLPORT(&(cap[i]->cap_port)))
				continue;
			++num_caps;
		}
		if (num_caps == 0) {
			fprintf(stderr, "Error: both caps are NULL\n");
			exit(1);
		} else if (num_caps == 1) {
			if (NULLPORT(&(cap[0]->cap_port)))
				cs_status = cs_singleton(&cs, cap[1]);
			else
				cs_status = cs_singleton(&cs, cap[0]);
		} else /* num_caps == 2 */ {
			suite *s = (suite *) malloc(2 * sizeof(suite));
			cs_status = (s != 0);
			if (cs_status) {
				s[0].s_current = 1;
				s[0].s_object = *(cap[0]);
				s[1].s_current = 1;
				s[1].s_object = *(cap[1]);
				cs.cs_initial = 1;
				cs.cs_final = 2;
				cs.cs_suite = s;
			}
		}
		if (!cs_status) {
			fprintf(stderr, "%s: can't malloc capset\n",
							argv[0]);
			exit(1);
		}
		if ((p = buf_put_capset(buf, buf + sizeof buf, &cs)) == 0) {
			fprintf(stderr, "%s: can't put capset\n", argv[0]);
			exit(1);
		}
		(void) fwrite(buf, 1, (size_t) (p - buf), stdout);
	} else if (delete_flag) {
		fprintf(stderr, "Okay to zero the caps? ");
		(void) gets(answer);
		if (answer[0] != 'y')
			exit(0);
		*(capability *)cp_cap0 = nullcap;
		*(capability *)cp_cap1 = nullcap;

		if ((err = disk_write(&supercap, L2VBLKSZ, blocknum,
					    (disk_addr) 1, buf)) != STD_OK) {
			fprintf(stderr,
				"%s: Can't write back to the disk (%s).\n",
							argv[0], err_why(err));
			exit(1);
		}
		fprintf(stderr, "Success.\n");
	}
	exit(0);
}
