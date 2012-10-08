/*	@(#)makecap.c	1.4	96/02/27 13:08:03 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** Makecap
**
**	This is a more serious attempt at a makecap that works like
**	the manual page says.
**  Author: Gregory J. Sharp, December 1990
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "module/name.h"
#include "module/prv.h"
#include "module/rnd.h"

extern int	optind;
extern char *	optarg;


int
usage(progname)
char *	progname;
{
    fprintf(stderr,
	    "Usage: %s [-r rights] [-o objectnumber] [-p putcapname] [getcapname]\n",
	    progname);
    return 1;
}


main(argc, argv)
int	argc;
char **	argv;
{
    capability	get_cap;
    port	check;
    errstat	err;
    int		c;
    rights_bits	mask;
    objnum	onum;
    char       *get_name, *put_name;

    onum = 0;
    mask = PRV_ALL_RIGHTS;
    get_name = NULL;
    put_name = NULL;

    while ((c = getopt(argc, argv, "o:p:r:")) != EOF)
	switch (c)
	{
	case 'r':
	    mask = strtol(optarg, (char **) 0, 0);
	    break;
	case 'o':
	    onum = strtol(optarg, (char **) 0, 0);
	    break;
	case 'p':
	    put_name = optarg;
	    break;
	default:
	    exit(usage(argv[0]));
	    /*NOTREACHED*/
	}
    
/* must be zero or 1 capability name arguments */
    if (optind < argc - 1)
	exit(usage(argv[0]));
    
/* if there is a capname and it isn't '-' then we use it to save the cap */
    if (optind == argc - 1 && strcmp(argv[optind], "-") != 0)
	get_name = argv[optind];

/* generate the get capability */
    uniqport(&get_cap.cap_port);
    uniqport(&check);
    if (prv_encode(&get_cap.cap_priv, onum, mask, &check) < 0)
    {
	fprintf(stderr, "%s: prv_encode of capability failed\n", argv[0]);
	exit(2);
    }

/* if put_name is nonzero, create and publish corresponding put capability */
    if (put_name != NULL) {
	capability put_cap;

	/* create put capability */
	put_cap.cap_priv = get_cap.cap_priv;
	priv2pub(&get_cap.cap_port, &put_cap.cap_port);
	
	if ((err = name_append(put_name, &put_cap)) != STD_OK)
	{
	    fprintf(stderr, "%s: can't store put capability `%s' (%s)\n",
		    argv[0], put_name, err_why(err));
	    exit(1);
	}
    }

/* either write the get_cap to stdout or store it in the directory server */
    if (get_name == NULL)
	(void) write(1, (char *) &get_cap, sizeof(get_cap));
    else
	if ((err = name_append(get_name, &get_cap)) != STD_OK)
	{
	    fprintf(stderr, "%s: can't store get capability `%s' (%s)\n",
		    argv[0], get_name, err_why(err));
	    exit(1);
	}
    exit(0);
}
