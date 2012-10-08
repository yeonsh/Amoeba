/*	@(#)pit.h	1.3	96/02/27 13:52:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* manifest constants */
#define	PIT_HZ		100		/* 100 ticks per second */
#define PIT_FREQ	1193182L	/* crystal frequency */
#define	PIT_INTERVAL	(1000 / PIT_HZ)	/* tick interval in msec */

/* interrupt request level and I/O ports */
#define	PIT_IRQ		0		/* timer's IRQ level */
#define PIT_CH0		0x40    	/* timer channel 0 */
#define PIT_CH2		0x42    	/* keyboard timer (channel 2) */
#define PIT_MODE	0x43    	/* timer mode control */

/* timer mode bits */
#define	PIT_LC		0x00		/* mode register for latch count */
#define	PIT_RB		0x03		/* mode register for read back */
#define	PIT_MODE3	0x36		/* mode 3: square wave rate generator */
