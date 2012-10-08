/*	@(#)modlist.h	1.1	96/02/27 10:02:49 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#ifndef MOD_LIST_H
#define MOD_LIST_H

errstat write_new_dir _ARGS((objnum obj, int modify));

errstat ml_write_dir _ARGS((objnum obj));
int	ml_has_mods _ARGS((objnum obj));
errstat	ml_store    _ARGS((objnum obj));
void	ml_remove   _ARGS((objnum obj));
void	ml_recover  _ARGS((sp_seqno_t *highest_seq));
void	ml_use      _ARGS((void));
errstat ml_init     _ARGS((int autoformat, int forceformat));
errstat ml_flush    _ARGS((void));

/* statistics, etc.: */
int	ml_in_use   _ARGS((void));
int	ml_nmods    _ARGS((void));
int	ml_time     _ARGS((void));
void	ml_print    _ARGS((void));

#endif /* MOD_LIST_H */
