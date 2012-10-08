/*	@(#)superblock.h	1.2	94/04/06 11:59:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

errstat sp_unmarshall_super _ARGS((char *buf, char *end, sp_block *super));
errstat sp_marshall_super   _ARGS((char *buf, char *end, sp_block *super));

#endif
