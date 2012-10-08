/*	@(#)b_create.c	1.4	94/04/07 10:59:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** b_create
**
**	The bullet server "create file" client stub.
**	It begins with a create transaction.  If the data in buf didn't
**	fit in one transaction then we have do MODIFY transactions after that.
**	We must not send "commit" until the last transaction.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "bullet/bullet.h"
#include "module/stdcmd.h"
#include "module/prv.h"

errstat
b_create(server, buf, size, commit, newfile)
capability *	server;		/* in: port for the bullet server */
char *		buf;		/* in: initial data for the file */
b_fsize		size;		/* in: #bytes data in buf */
int		commit;		/* in: the commit and safety flags */
capability *	newfile;	/* out: capability for created file */
{
    header	hdr;
    b_fsize	offset;
    b_fsize	sendsize;
    bufsize	t;

    if (size < 0)
	return STD_ARGBAD;

    hdr.h_port = server->cap_port;
    hdr.h_priv = server->cap_priv;
    hdr.h_command = BS_CREATE;		/* first trans does create */
    offset = 0;
    do
    {
	if (size > BS_REQBUFSZ)
	{
	    hdr.h_size = BS_REQBUFSZ;
	    hdr.h_extra = 0;
	}
	else	/* last transaction so send commit flags */
	{
	    hdr.h_size = size;
	    hdr.h_extra = commit;
	}
	sendsize = hdr.h_size;
	if ((t = trans(&hdr, buf, hdr.h_size, &hdr, NILBUF, 0)) != STD_OK)
	    return ERR_CONVERT(t);
	if (hdr.h_status != STD_OK)
	{
	    if (!NULLPORT(&hdr.h_port) &&
		    prv_number(&hdr.h_priv) != prv_number(&server->cap_priv))
	    {
		/* then a new file was half made */
		newfile->cap_port = hdr.h_port;
		newfile->cap_priv = hdr.h_priv;
		(void) std_destroy(newfile);
	    }
	    return ERR_CONVERT(hdr.h_status);
	}
	buf += sendsize;
	offset += sendsize;
	if ((size -= sendsize) > 0)	/* then we need to append more data */
	{
	    hdr.h_command = BS_MODIFY;
	    hdr.h_offset = offset;
	}
    } while (size > 0);

/* give the user the new capability */
    newfile->cap_port = hdr.h_port;
    newfile->cap_priv = hdr.h_priv;
    return STD_OK;
}
