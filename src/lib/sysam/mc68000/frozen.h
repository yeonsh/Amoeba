/*	@(#)frozen.h	1.1	96/02/27 11:26:04 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __FROZEN_H__
#define	__FROZEN_H__

/* The argument offsets are dependent on FROZEN_SUPPORT. */
#ifdef FROZEN_SUPPORT
#define FROZEN_SPACE	4
#else
#define FROZEN_SPACE	0
#endif

#define OFF_HDR		EXPR(8+FROZEN_SPACE)
#define OFF_BUF		EXPR(12+FROZEN_SPACE)
#define OFF_CNT		EXPR(16+FROZEN_SPACE)

#endif /* __FROZEN_H__ */
