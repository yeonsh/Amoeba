/*	@(#)superfile.h	1.3	96/02/27 10:23:27 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef SUPER_FILE_H
#define SUPER_FILE_H

#include "_ARGS.h"

#include "soap/super.h"

/*
 * Initilisation routine.  Returns get capabilities stored in block 0.
 */
void super_init _ARGS((capability *generic,
		       capability *my_super, capability *other_super));

/*
 * getting, setting and storing of [1|2] copymode
 */
void set_copymode	_ARGS((int mode));
int  get_copymode	_ARGS((void));
void flush_copymode	_ARGS((void));

/*
 * getting and setting of seqence number
 */
void set_seq_nr	_ARGS((long seq));
long get_seq_nr	_ARGS((void));

/*
 * mapping of object number to directory file replicas
 */
void get_dir_caps _ARGS((objnum obj, capability filecaps[NBULLETS]));
void set_dir_caps _ARGS((objnum obj, capability filecaps[NBULLETS]));

/*
 * Function write_blocks_modified writes the super blocks that were
 * marked modified as a result of set_dir_caps() to disk.
 */
void write_blocks_modified _ARGS((void));

/*
 * Copy_super_blocks makes sure the other side has the same super
 * blocks as ourselves.
 * If the other side has the same sequence, nothing needs to be done.
 */
errstat copy_super_blocks _ARGS((void));
int got_super_block	  _ARGS((header *hdr, char *buf, int size));

/*
 * Interface to create, retrieve, store and throw away intentions from
 * the part of the superfile currently used to store intentions.
 */
int  super_nintents	 _ARGS((void));
int  super_avail_intents _ARGS((void));
int  super_add_intent	 _ARGS((objnum obj, capability filecaps[]));
void super_get_intent	 _ARGS((int intent_nr, sp_new *intent));
void super_reset_intents _ARGS((int nintents));
void super_store_intents _ARGS((void));

#endif

