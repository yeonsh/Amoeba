/*	@(#)chbul.c	1.3	94/04/06 11:56:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** CHBUL
**	This program is for system administrators to change the bullet
**	server currently being used by the specified soap server.
**	The parameters to the function are
**		the private capability of the soap server
**		the super capability of the new bullet server to use
**		the number of the bullet server to be replaced (either 0 or 1)
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "stdio.h"
#include "stdlib.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "soap/super.h"
#include "module/name.h"
#include "module/buffers.h"

main(argc, argv)
int	argc;
char *	argv[];
{
    errstat	sp_changebullet();

    capability	spcap;
    capability	bulletcap;
    int		bulletnum;
    errstat	err;

    if (argc != 4)
    {
	fprintf(stderr, "Usage: %s soap-cap bullet-cap bullet-number\n",
								    argv[0]);
	exit(-1);
    }

/* look up private capability of the soap server */
    if ((err = name_lookup(argv[1], &spcap)) != STD_OK)
    {
	fprintf(stderr, "%s: look up of '%s' failed: %s\n",
						argv[0], argv[1], err_why(err));
	exit(-2);
    }

/* look up super capability of the new bullet server to be used */
    if ((err = name_lookup(argv[2], &bulletcap)) != STD_OK)
    {
	fprintf(stderr, "%s: look up of '%s' failed: %s\n",
						argv[0], argv[2], err_why(err));
	exit(-2);
    }

    bulletnum = atoi(argv[3]);
    if (bulletnum != 0 && bulletnum != 1)
    {
	fprintf(stderr, "%s: bullet server number must be either 0 or 1\n",
								    argv[0]);
	exit(-3);
    }

    if ((err = sp_changebullet(&spcap, &bulletcap, bulletnum)) != STD_OK)
    {
	fprintf(stderr, "%s: attempt to change bullet server failed: %s\n",
							argv[0], err_why(err));
	exit(-4);
    }
    exit(0);
}


errstat
sp_changebullet(spcap, bulcap, bulnum)
capability *	spcap;
capability *	bulcap;
int		bulnum;
{
    header	hdr;
    char	buf[2 * sizeof (capability)];
    bufsize	retval;
    char *	p;

    hdr.h_port = spcap->cap_port;
    hdr.h_priv = spcap->cap_priv;
    hdr.h_command = SP_NEWBULLET;
    hdr.h_extra = bulnum;
    if ((p = buf_put_cap(buf, buf + 2*sizeof (capability), bulcap)) == 0)
	return STD_SYSERR;
    retval = trans(&hdr, (bufptr) buf, (bufsize) (p - buf), &hdr, NILBUF, 0);
    if (retval != 0)
	return ERR_CONVERT(retval);
    return ERR_CONVERT(hdr.h_status);
}
