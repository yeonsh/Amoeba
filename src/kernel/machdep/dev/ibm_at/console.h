/*	@(#)console.h	1.5	94/04/06 09:17:47 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * The following defines toggle certain options in the console driver.
 * The CRT_LINEWRAP option turns on the 80 column wrap feature. The
 * CRT_BLANKTIME determines the number milliseconds without any screen
 * or keyboard activity before the screen should be blanked.
 */
#define	CRT_LINEWRAP		1		/* wrap line in column 80 */
#define	CRT_BLANKTIME		(5*60*1000)	/* time before screen blank */

/* fundamental constants */
#define	CRT_TABMASK		07	/* mask for column when tabbing */

/* fundamental constants for ANSI state machine */
#define	ANSI_NPARAMS		2	/* # of parameters for ANSI sequences */

/* various adapter types */
#define CRT_TYPE_UNKNOWN	0	/* no idea what it is */
#define CRT_TYPE_CGA40		1	/* CGA 40 columns mode */
#define CRT_TYPE_CGA80		2	/* CGA 80 columns mode */
#define CRT_TYPE_MONO80		3	/* monochrome, 80 columns */
#define CRT_TYPE_HERCULES	4	/* hercules adapter */
#define CRT_TYPE_EGA		5	/* EGA */
#define CRT_TYPE_VGA		6	/* VGA */

/* 6845 registers */
#define	CRT_INDEX		4	/* index register */
#define	CRT_DATA		5	/* data register */
#define	CRT_MODE		8	/* mode register */
#define	CRT_COLOR		9	/* color register */
#define	CRT_STATUS		10	/* status register */
#define	CRT_STARTHIGH		12	/* start address, high word */
#define	CRT_STARTLOW		13	/* start address, low word */
#define	CRT_CURSOR		14	/* cursor register */

/* bits in mode register */
#define	CRT_MODE_ALPHA40	0x00	/* 40x25 alpha numeric */
#define	CRT_MODE_ALPHA80	0x01	/* 80x25 alpha numeric */
#define	CRT_MODE_GRAPHICS	0x02	/* graphis mode */
#define	CRT_MODE_BLACKWHITE	0x04	/* black & white */
#define	CRT_MODE_ENABLE		0x08	/* video enable */
#define	CRT_MODE_HIGHRES	0x10	/* high resolution graphics */
#define	CRT_MODE_BLINK		0x20	/* enable blinking */

/* bits in status register */
#define	CRT_STATUS_UPDATE	0x01	/* safe to update buffer */
#define	CRT_STATUS_VSYNC	0x08	/* vertical sync in progress */

/* mono video card */
#define	CRT_MONO6845		0x3B0	/* port for 6845 mono */
#define	CRT_MONOMASK		0x0FFF	/* mask for 4Kb video ram */
#define	CRT_MONOBASE		0xB0000	/* start video memory */

/* color video card */
#define	CRT_COLOR6845		0x3D0	/* port for 6845 color */
#define	CRT_COLORMASK		0x3FFF	/* mask for 16Kb video ram */
#define	CRT_COLORBASE		0xB8000	/* start video memory */

/* EGA/VGA definitions */
#define VGA_SEQREG		0x3C4	/* sequencer registers */
#define VGA_SEQ_CLOCKINGMODE	1	/* clocking mode index */
#define VGA_SEQ_MEMMODE		4	/* memory mode */

#define	VGA_MISCOUTPUT		0x3CC	/* miscelanous output register */
