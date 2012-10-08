/*	@(#)tape.h	1.3	96/02/27 10:34:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_TAPE_H__
#define __MODULE_TAPE_H__

#include "server/tape/tape.h"

#define	tape_erase	_tape_erase
#define	tape_fpos	_tape_fpos
#define	tape_fskip	_tape_fskip
#define	tape_load	_tape_load
#define	tape_read	_tape_read
#define	tape_rewind	_tape_rewind
#define	tape_rpos	_tape_rpos
#define	tape_rskip	_tape_rskip
#define	tape_status	_tape_status
#define	tape_unload	_tape_unload
#define	tape_why	_tape_why
#define	tape_write_eof	_tape_write_eof
#define	tape_write	_tape_write

errstat		tape_erase _ARGS((capability *));
errstat		tape_fpos _ARGS((capability *, int32 *));
errstat		tape_fskip _ARGS((capability *, int32));
errstat		tape_load _ARGS((capability *));
errstat		tape_read _ARGS((capability *, bufptr, bufsize, bufsize *));
errstat		tape_rewind _ARGS((capability *, int));
errstat		tape_rpos _ARGS((capability *, int32 *));
errstat		tape_rskip _ARGS((capability *, int32));
errstat		tape_status _ARGS((capability *, bufptr, bufsize));
errstat		tape_unload _ARGS((capability *));
char *		tape_why _ARGS((errstat));
errstat		tape_write_eof _ARGS((capability *));
errstat		tape_write _ARGS((capability *, bufptr, bufsize, bufsize *));

#endif /* __MODULE_TAPE_H__ */
