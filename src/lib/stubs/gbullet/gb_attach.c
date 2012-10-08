/*	@(#)gb_attach.c	1.1	96/02/27 11:14:06 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** battach
**	Bullet server administrator's command add a member to a group bullet
**	server.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/name.h"
#include "ampolicy.h"

#include "module/disk.h"
#include "gbullet/bullet.h"

#include "stdlib.h"


errstat
gb_attach(svrcap,newcap)
capability *svrcap;
capability *newcap;
{
    header  hdr;
    errstat err;
    bufsize siz;


    hdr.h_port = svrcap->cap_port;
    hdr.h_priv = svrcap->cap_priv;
    hdr.h_command = BS_ATTACH;
    hdr.h_size= sizeof(capability);
    hdr.h_offset= 0;
    hdr.h_extra= 0;
    
    siz = trans(&hdr, (bufptr) newcap, hdr.h_size,
		&hdr, (bufptr) NULL, (bufsize) 0);
    if (ERR_STATUS(siz)) {
        err = ERR_CONVERT(siz);
    } else {
        err = ERR_CONVERT(hdr.h_status);
    }

    return err;
}
