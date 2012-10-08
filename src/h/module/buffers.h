/*	@(#)buffers.h	1.5	96/02/27 10:32:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_BUFFERS_H__
#define __MODULE_BUFFERS_H__

#include <_ARGS.h>
#include <stdlib.h> /* for size_t */

#define	buf_get_bytes	_buf_get_bytes
#define	buf_put_bytes	_buf_put_bytes
#define	buf_get_cap	_buf_get_cap
#define	buf_put_cap	_buf_put_cap
#define	buf_get_long	_buf_get_long
#define	buf_put_long	_buf_put_long
#define	buf_get_port	_buf_get_port
#define	buf_put_port	_buf_put_port
#define	buf_get_priv	_buf_get_priv
#define	buf_put_priv	_buf_put_priv
#define	buf_get_short	_buf_get_short
#define	buf_put_short	_buf_put_short
#define	buf_get_char	_buf_get_char
#define	buf_put_char	_buf_put_char
#define	buf_get_string	_buf_get_string
#define	buf_put_string	_buf_put_string

char *	buf_get_bytes	_ARGS((char *p, char *endbuf, void *b, size_t n));
char *	buf_put_bytes	_ARGS((char *p, char *endbuf, void *b, size_t n));
char *	buf_get_cap	_ARGS((char *p, char *endbuf, capability *cap));
char *	buf_put_cap	_ARGS((char *p, char *endbuf, capability *cap));
char *	buf_get_long	_ARGS((char *p, char *endbuf, long *val));
char *	buf_put_long	_ARGS((char *p, char *endbuf, long val));
char *	buf_get_port	_ARGS((char *p, char *endbuf, port *ptr));
char *	buf_put_port	_ARGS((char *p, char *endbuf, port *ptr));
char *	buf_get_priv	_ARGS((char *p, char *endbuf, private *priv));
char *	buf_put_priv	_ARGS((char *p, char *endbuf, private *priv));
char *	buf_get_short	_ARGS((char *p, char *endbuf, short *val));
char *	buf_put_short	_ARGS((char *p, char *endbuf, short val));
char *	buf_get_char	_ARGS((char *p, char *endbuf, char *val));
char *	buf_put_char	_ARGS((char *p, char *endbuf, char val));
char *	buf_get_string	_ARGS((char *p, char *endbuf, char **s));
char *	buf_put_string	_ARGS((char *p, char *endbuf, const char *s));

#define buf_get_uint32(p,e,dp)	buf_get_int32(p,e,(int32 *)dp)
#define buf_put_uint32(p,e,d)	buf_put_int32(p,e,(int32)d)
#define buf_get_uint16(p,e,dp)	buf_get_int16(p,e,(int16 *)dp)
#define buf_put_uint16(p,e,d)	buf_put_int16(p,e,(int16)d)
#define buf_get_uint8(p,e,dp)	buf_get_int8(p,e,(int8 *)dp)
#define buf_put_uint8(p,e,d)	buf_put_int8(p,e,(int8)d)

#endif /* __MODULE_BUFFERS_H__ */
