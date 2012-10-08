/*	@(#)mouse.h	1.3	94/04/06 09:21:34 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Default parameters for serial mice */
#define	MOUSE_ASY_IRQ	    4		  /* IRQ level */
#define	MOUSE_ASY_BAUDRATE  1200	  /* baud rate */
#define	MOUSE_ASY_REG	    ASY_LINE0	  /* serial line I/O port COM0 */
#define	MOUSE_ASY_LCR	    ASY_LCR_8BIT  /* 8 bits, 1 stop bit, no parity */

/* More defines for reading the serial port */
#define LSR_RCV_MASK    0x1f

/* Logitech bus mouse IRQ vector */
#define	LB_IRQ		5		/* default is irq 3 */

/* Logitech bus mouse register assigment */
#define LB_DATAREG	0x23C		/* data register (R) */
#define LB_SIGNREG	0x23D		/* signature register (R/W) */
#define LB_CNTLREG	0x23E		/* control register (W) */
#define LB_INTRREG	0x23E		/* interrupt register (R) */
#define LB_CONFREG	0x23F		/* coniguration register (R/W) */

/* bits in control register */
#define	LB_ENABLE	0x00		/* enable interrupts */
#define	LB_DISABLE	0x10		/* disable interrupt */
#define	LB_SHL		0x20		/* select high/low nibble */
#define	LB_SXY		0x40		/* select X/Y counter */
#define	LB_HC		0x80		/* Hold counters (latch on edge) */

/* bits in interrupt register */
#define LB_IRQ2		0x01		/* irq level 2 */
#define LB_IRQ3		0x02		/* irq level 3 */
#define LB_IRQ4		0x04		/* irq level 4 */
#define LB_IRQ5		0x08		/* irq level 5 */

/* bits in configuration register */
#define	LB_CONFIG	0x91		/* some magic number */

