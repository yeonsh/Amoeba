/*	@(#)dirmarshall.h	1.3	94/04/06 11:57:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef DIR_MARSHALL_H
#define DIR_MARSHALL_H

#include "_ARGS.h"

char *buf_put_row   _ARGS((char *p, char *e, struct sp_row *sr, int ncols));
char *buf_put_dir_n _ARGS((char *p, char *e, struct sp_dir *sd, int n,
			   char **nrows_p));
char *buf_put_dir   _ARGS((char *p, char *e, struct sp_dir *sd));

char *buf_get_row   _ARGS((char *p, char *e ,struct sp_row *sr,
			   int ncols, errstat *err_p));
char *buf_get_dir   _ARGS((char *p, char *e, struct sp_dir **psd,
			   errstat *err_p));

char *buf_put_seqno _ARGS((char *buf, char *end, sp_seqno_t *seqno));
char *buf_get_seqno _ARGS((char *buf, char *end, sp_seqno_t *seqno));

#define SP_DIR_MAGIC	0x05385218

#endif /* DIR_MARSHALL_H */
