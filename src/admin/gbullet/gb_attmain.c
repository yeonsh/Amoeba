/*	@(#)gb_attmain.c	1.1	96/02/27 10:01:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 

/*
 * Author: Ed Keizer
 */
 
/*
** gb_attach
**
**	This program sends the specified bullet server a new member
**	command.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/name.h"
#include "gbullet/bullet.h"
#include "stdio.h"
#include "stdlib.h"

main(argc, argv)
int	argc;
char *	argv[];
{
    errstat	err;
    capability	bulletsvr;
    capability	newsvr;

    if (argc != 3)
    {
	fprintf(stderr, "Usage: %s bullet_server_cap new_member_cap\n", argv[0]);
	exit(1);
    }

    if ((err = name_lookup(argv[1], &bulletsvr)) != STD_OK)
    {
	fprintf(stderr, "%s: lookup of '%s' failed: %s\n",
						argv[0], argv[1], err_why(err));
	exit(1);
    }

    if ((err = name_lookup(argv[2], &newsvr)) != STD_OK)
    {
	fprintf(stderr, "%s: lookup of '%s' failed: %s\n",
						argv[0], argv[2], err_why(err));
	exit(1);
    }

    if ((err = gb_attach(&bulletsvr,&newsvr)) != STD_OK)
    {
	fprintf(stderr, "%s: group bullet attach failed: %s\n",
							argv[0], err_why(err));
	exit(1);
    }

    exit(0);
}
