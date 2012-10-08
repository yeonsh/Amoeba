/*	@(#)gb_state.c	1.1	96/02/27 11:14:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** bstate
**	Bullet server administrator's command to alter member status's
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/name.h"
#include "ampolicy.h"

#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"

#include "stdlib.h"


errstat
gb_state(svrcap,member,flag)
capability *svrcap;
int	member;
int	flag;
{
    header  hdr;
    errstat err;
    bufsize siz;


    hdr.h_port = svrcap->cap_port;
    hdr.h_priv = svrcap->cap_priv;
    hdr.h_command = BS_STATE;
    hdr.h_size= 0;
    hdr.h_offset= member;
    hdr.h_extra= flag;
    
    siz = trans(&hdr, (bufptr) NULL, 0, &hdr, (bufptr) NULL, (bufsize) 0);
    if (ERR_STATUS(siz)) {
        err = ERR_CONVERT(siz);
    } else {
        err = ERR_CONVERT(hdr.h_status);
    }

    return err;
}
