/*	@(#)sun4c_auxio.h	1.2	94/04/06 09:32:21 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SUN4C_AUXIO_H__
#define	__SUN4C_AUXIO_H__

/*
 * The various fields in the sun4c auxilart I/O register.
 */

#define	AUXIO_LED		0x01	/* The little led on the front */
#define	AUXIO_FLOPPY_EJECT	0x02	/* To eject the floppy */
#define	AUXIO_FLOPPY_TC		0x04	/* Terminal count */
#define	AUXIO_FLOPPY_SELECT	0x08	/* Drive select */
#define	AUXIO_FLOPPY_CHANGED	0x10	/* 1 = New floppy was inserted */
#define	AUXIO_FLOPPY_DENSITY	0x20	/* 1 = High, 0 = Low */
/* Whenever writing to the auxio reg the following bits must always be 1 !! */
#define	AUXIO_MUST_BE_ON	0xF0

extern void set_auxioreg _ARGS(( unsigned int bits, int state ));
extern uint32  get_auxioreg _ARGS(( void ));

#endif /*__SUN4C_AUXIO_H__*/
