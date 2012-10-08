/*	@(#)modnvram.h	1.1	96/02/27 10:02:52 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#ifndef MOD_NOVRAM_H
#define MOD_NOVRAM_H

char   *novr_get_buf     _ARGS((void));
errstat novr_writeblock  _ARGS((disk_addr block));
errstat novr_readblock   _ARGS((disk_addr block, char **bufp));
errstat novr_init        _ARGS((capability *servercap, disk_addr firstblock,
				disk_addr *nblocks, unsigned int *size));

/* entrypoint to create an empty modlist: */
errstat novr_format      _ARGS((capability *servercap, disk_addr firstblock,
				disk_addr nblocks));

#endif /* MOD_NOVRAM_H */
