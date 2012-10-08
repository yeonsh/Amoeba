/*	@(#)sp_impl.h	1.4	96/02/27 10:23:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef SP_IMPL_H
#define SP_IMPL_H

#include "_ARGS.h"

extern int sp_save_and_mod_dir  _ARGS((sp_tab *st, header *hdr,
				       short rownum, sp_row *row));
extern int sp_undo_mod_dir	_ARGS((sp_tab *st));

extern int sp_trans		_ARGS((header *hdr, char *buf, int size,
				       char *outbuf, int outsize));

extern void sp_add_to_freelist	_ARGS((sp_tab *st));
extern void sp_clear_updates	_ARGS((void));
extern void sp_free_mods	_ARGS((sp_tab *st));
extern void free_dir		_ARGS((struct sp_dir *sd));

extern int  sp_allocated_rows	_ARGS((void));
extern void sp_update_nrows	_ARGS((int delta_rows));
extern int  sp_rows_to_allocate _ARGS((int nrows));

extern sp_tab *sp_first_update  _ARGS((void));
extern sp_tab *sp_next_update   _ARGS((void));

extern unsigned int sp_twopower	_ARGS((int i));

#ifdef SOAP_GROUP
void free_row _ARGS((struct sp_row *sr));
#endif

#endif
