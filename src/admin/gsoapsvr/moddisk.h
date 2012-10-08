/*	@(#)moddisk.h	1.1	96/02/27 10:02:46 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#ifndef MOD_DISK_H
#define MOD_DISK_H

char   *md_get_buf     _ARGS((void));
errstat md_writeblock  _ARGS((disk_addr block));
errstat md_readblock   _ARGS((disk_addr block, char **bufp));
errstat md_init        _ARGS((capability *servercap, disk_addr firstblock,
			      disk_addr *nblocks, unsigned int *size));

#ifdef MAKESUPER
/* temporary hack to create empty modlist: */
errstat md_blockformat _ARGS((capability *servercap, disk_addr firstblock,
			      disk_addr nblocks));
#endif

#endif /* MOD_DISK_H */
