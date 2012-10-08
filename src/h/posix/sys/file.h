/*	@(#)file.h	1.2	94/04/06 16:56:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FILE_H__
#define __FILE_H__

/* <sys/file.h> for BSD programs */

#include "../fcntl.h"
#include "../unistd.h"

#define L_SET	SEEK_SET
#define L_INCR	SEEK_CUR
#define L_XTND	SEEK_END

#endif /* __FILE_H__ */
