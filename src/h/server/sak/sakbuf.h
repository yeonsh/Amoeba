/*	@(#)sakbuf.h	1.4	96/02/27 10:38:07 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SVR_SAKBUF_H__
#define __SVR_SAKBUF_H__

struct sak_job_options;
struct trans_info;
struct caplist;

#define	buf_get_sak_job_options	_buf_get_sak_job_options
#define	buf_put_sak_job_options	_buf_put_sak_job_options
#define	buf_get_sched		_buf_get_sched
#define	buf_put_sched		_buf_put_sched
#define	buf_get_trans_info	_buf_get_trans_info
#define	buf_put_trans_info	_buf_put_trans_info
#define	buf_put_trans		_buf_put_trans
#define	buf_get_trans		_buf_get_trans
#define	buf_put_argvptr		_buf_put_argvptr
#define	buf_get_argvptr		_buf_get_argvptr
#define	buf_free_argvptr	_buf_free_argvptr
#define	buf_put_caplptr		_buf_put_caplptr
#define	buf_get_caplptr		_buf_get_caplptr
#define	buf_free_caplptr	_buf_free_caplptr

char *buf_get_sak_job_options _ARGS((char *, char *, struct sak_job_options *));
char *buf_put_sak_job_options _ARGS((char *, char *, struct sak_job_options *));

char *buf_get_sched _ARGS((char *, char *, int8 **, errstat *));
char *buf_put_sched _ARGS((char *, char *, int8 **));

char *buf_get_trans_info _ARGS((char *, char *, struct trans_info *));
char *buf_put_trans_info _ARGS((char *, char *, struct trans_info *));

char *buf_put_int16 _ARGS((char *, char *, int16));
char *buf_get_int16 _ARGS((char *, char *, int16 *));

char *buf_put_int32 _ARGS((char *, char *, int32));
char *buf_get_int32 _ARGS((char *, char *, int32 *));

char *buf_put_trans _ARGS((char *, char *, header *));
char *buf_get_trans _ARGS((char *, char *, header *));

char *buf_put_argvptr _ARGS((char *, char *, char **));
char *buf_get_argvptr _ARGS((char *, char *, char ***, int, errstat *));
void buf_free_argvptr _ARGS((char **));

char *buf_put_caplptr _ARGS((char *, char *, struct caplist *));
char *buf_get_caplptr _ARGS((char *, char *, struct caplist **, int, errstat *));
void buf_free_caplptr _ARGS((struct caplist *));

#define	buf_get_command		buf_get_uint16
#define	buf_put_command		buf_put_uint16

#define	buf_get_bufsize		buf_get_uint16
#define	buf_put_bufsize		buf_put_uint16

#define	buf_get_interval	buf_get_int32
#define	buf_put_interval	buf_put_int32

#define	buf_get_int8		buf_get_char
#define	buf_put_int8		buf_put_char

#include "module/buffers.h"

#endif /* __SVR_SAKBUF_H__ */
