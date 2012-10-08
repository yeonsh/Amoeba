/*	@(#)pic.h	1.2	94/04/06 09:22:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* command codes */
#define	PIC_NS_EOI	0x20		/* non-specific end of interrupt */

#if defined(ISA)
/* register addresses */
#define PIC_INT1_CMD	0x20		/* first interrupt controller */
#define PIC_INT1_IMR	0x21		/* interrupt mask register */
#define PIC_INT2_CMD	0xA0		/* second interrupt controller */
#define PIC_INT2_IMR	0xA1		/* interrupt mask register */

/* initialization codes */
#define	PIC_ICW1	0x11		/* edge triggered, cascade, ICW4 */
#define	PIC_ICW2_MASTER	IRQ0_VECTOR	/* master interrupt base */
#define	PIC_ICW3_MASTER	0x04		/* bit 2 for slave on channel 2 */
#define	PIC_ICW4_MASTER	0x01		/* not SFNM, not buffered, normal EOI */
#define	PIC_ICW2_SLAVE	IRQ8_VECTOR	/* slave interrupt base */
#define	PIC_ICW3_SLAVE	0x02		/* slave identity is 2 */
#define	PIC_ICW4_SLAVE	0x01		/* not SFNM, not buffered, normal EOI */
#define	PIC_OCW3	0x0B		/* read IS register */
#endif

#if defined(MCA)
/* register addresses */
#define PIC_INT1_CMD	0x20		/* first interrupt controller */
#define PIC_INT1_IMR	0x21		/* interrupt mask register */
#define PIC_INT2_CMD	0xA0		/* second interrupt controller */
#define PIC_INT2_IMR	0xA1		/* interrupt mask register */

/* initialization codes */
#define	PIC_ICW1	0x19		/* edge triggered, no cascade, ICW4 */
#define	PIC_ICW2_MASTER	IRQ0_VECTOR	/* master interrupt base */
#define	PIC_ICW3_MASTER	0x04		/* bit 2 for slave on channel 2 */
#define	PIC_ICW4_MASTER	0x01		/* not SFNM, not buffered, normal EOI */
#define	PIC_ICW2_SLAVE	IRQ8_VECTOR	/* slave interrupt base */
#define	PIC_ICW3_SLAVE	0x02		/* slave identity is 2 */
#define	PIC_ICW4_SLAVE	0x01		/* not SFNM, not buffered, normal EOI */
#define	PIC_OCW3	0x0B		/* read IS register */
#endif

