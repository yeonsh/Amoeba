/*	@(#)sun4m_auxio.h	1.1	94/05/17 11:21:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
*/

#ifndef	__SUN4M_AUXIO_H__
#define	__SUN4M_AUXIO_H__

#define	AUX_LED			0x01
#define	AUX_FLOPPY_TC		0x02
#define	AUX_MMMUX		0x04
#define	AUX_LINKTEST		0x08
#define	AUX_FLOPPY_DENSITY	0x20

void		set_auxioreg _ARGS(( unsigned int bits ));
unsigned int	get_auxioreg _ARGS(( void ));

#endif /* __SUN4M_AUXIO_H__ */
