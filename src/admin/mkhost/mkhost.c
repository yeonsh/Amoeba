/*	@(#)mkhost.c	1.6	96/02/27 10:16:56 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** mkhost pathname ethernet-address
**
** Creates a host capability and stores it in pathname (at the Amoeba
** directory server).
**
** Specifying '-' as pathname writes it on stdout.
*/

#include <amtools.h>
#include "capset.h"
#include "soap/soap.h"

main(argc, argv)
	int argc;
	char **argv;
{
	port p;
	port chkf;
	capability cap;
	errstat err;
	int new_kernel_security;
	rights_bits rightbits;
	
	if (argc != 3 && argc != 4) {
		fprintf(stderr, "usage: %s pathname ethernet-address\n",
								    argv[0]);
		fprintf(stderr, "   or: %s pathname get-port check-field\n",
								    argv[0]);
		fprintf(stderr, "(pathname '-' writes to stdout)\n");
		exit(2);
	}
	
	if (geteaddr(argv[2], (char *) &p) < 0) {
		fprintf(stderr,
		"%s: eaddr format is xx-xx-xx-xx-xx-xx or xx:xx:xx:xx:xx:xx\n",
								    argv[0]);
		exit(2);
	}

	if (argc == 3) {
		new_kernel_security = 0;
		chkf = p;
	} else {
		if (geteaddr(argv[3], (char *) &chkf) < 0) {
			fprintf(stderr,
			"%s: eaddr format is xx-xx-xx-xx-xx-xx or xx:xx:xx:xx:xx:xx\n",
								    argv[0]);
			exit(2);
		}
		new_kernel_security = 1;
	}
	
	rightbits = PRV_ALL_RIGHTS;
	if (!new_kernel_security)
		rightbits &= ~(SP_MODRGT | SP_DELRGT);
	priv2pub(&p, &cap.cap_port);
	if (prv_encode(&cap.cap_priv, (objnum) 1, rightbits, &chkf) < 0) {
		fprintf(stderr, "%s: prv_encode failed\n", argv[0]);
		exit(1);
	}
	
	if (strcmp(argv[1], "-") == 0)
		(void) write(1, (char *) &cap, sizeof cap);
	else {
		if ((err = name_append(argv[1], &cap)) != STD_OK) {
			fprintf(stderr, "%s: can't create %s (%s)\n",
					    argv[0], argv[1], err_why(err));
			exit(1);
		}
	}
	
	exit(0);
}

int
geteaddr(str, p)
	char *str;
	register char *p;
{
	int byte[6];
	char dum;
	register int *b;
	char *format;
	
	if (strchr(str, '-') != NULL)
		format = "%2x-%2x-%2x-%2x-%2x-%2x%c";
	else if (strchr(str, ':') != NULL)
		format = "%2x:%2x:%2x:%2x:%2x:%2x%c";
	else
		return -1;
	
	if (sscanf(str, format,
		&byte[0], &byte[1], &byte[2], &byte[3], &byte[4], &byte[5],
		&dum) != 6)
		return -1;
	b = byte;
	*p++ = *b++;
	*p++ = *b++;
	*p++ = *b++;
	*p++ = *b++;
	*p++ = *b++;
	*p++ = *b++;
	return 0;
}
