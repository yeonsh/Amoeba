/*	@(#)systask.h	1.6	96/02/27 10:38:34 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SYSTASK_H__
#define __SYSTASK_H__

/*
 *	Commands specific to the systhread
 */

#define	SYS_PRINTBUF	(SYS_FIRST_COM + 0)
#define	SYS_KSTAT	(SYS_FIRST_COM + 1)
#define	SYS_NETCONTROL	(SYS_FIRST_COM + 3)
#define	SYS_BOOT	(SYS_FIRST_COM + 4)
#define	SYS_CHGCAP	(SYS_FIRST_COM + 5)

/*
 *	Rights bits for the systhread
 */

#define	SYS_RGT_ALL		0xFF
#define	SYS_RGT_NONE		0x00
#define	SYS_RGT_READ		0x01	/* Some kstats & printbuf */
#define	SYS_RGT_CONTROL		0x02	/* Net control */
#define	SYS_RGT_BOOT		0x10	/* Right to download a new kernel */

#endif /* __SYSTASK_H__ */
