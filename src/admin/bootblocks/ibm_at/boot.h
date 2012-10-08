/*	@(#)boot.h	1.3	94/04/06 11:29:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * boot.h
 */

/*
 * Bootblok copies itself at this address to make room for the actual
 * kernel. There are two possible locations to position the boot program:
 * just below the 512Kb or 640Kb limit depending on how much memory the
 * machine has installed. 
 */
#ifdef BOOT512
#define	BOOTADDR    0x7DA00	/* (512Kb - 19 * 512b) */
#define	BOOTSEG     0x07DA0
#endif
#ifdef BOOT640
#define	BOOTADDR    0x9DA00	/* (640Kb - 19 * 512b) */
#define	BOOTSEG     0x09DA0
#endif

/* boot address constants */
#define	LOADADDR    0x02000	/* address at which the kernel is loaded */
#define	LOADSEG	    0x00200	/* segment at which the kernel is loaded */
#define	BIOSSEG     0x007C0	/* where the bios loads the bootblok */
#define	STACKOFFSET 0x2600	/* start stack (19 * 512b) */

/* fundamental constants */
#define LOWMEMSIZE  (640*1024L)	/* maximum low memory size */

/* various masks */
#define	OFF_MASK    0x000F	/* offset mask for physb to hclick:offset */
#define	HCHIGH_MASK 0x0F	/* h/w click mask for low byte of hi word */
#define	HCLOW_MASK  0xF0	/* h/w click mask for low byte of low word */

/* boot disk id's */
#define	HDID	    0x80	/* boot hard disk id */
#define	FLID	    0x00	/* boot floppy disk id */

