/*	@(#)dma.h	1.2	94/04/06 09:18:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* chip selection macros */
#define	DMA_CHIP1(ch)	((ch) >= 0 && (ch) <= 3)
#define	DMA_CHIP2(ch)	((ch) >= 4 && (ch) <= 7)

/* register offsets for the first 8237A DMA chip (four 8-bit channels) */
#define	DMA1_BASE0	0x00	/* base channel 0 */
#define	DMA1_COUNT0	0x01	/* count channel 0 */
#define	DMA1_BASE1	0x02	/* base channel 1 */
#define	DMA1_COUNT1	0x03	/* count channel 1 */
#define	DMA1_BASE2	0x04	/* base channel 2 */
#define	DMA1_COUNT2	0x05	/* count channel 2 */
#define	DMA1_BASE3	0x06	/* base channel 3 */
#define	DMA1_COUNT3	0x07	/* count channel 3 */
#define	DMA1_CMD	0x08	/* command status */
#define	DMA1_REQ	0x09	/* request register */
#define	DMA1_SMASK	0x0A	/* single mask */
#define	DMA1_MODE	0x0B	/* mode register */
#define	DMA1_FF		0x0C	/* internal flip-flop */

/* register offsets for the second 8237A DMA chip (four 16-bit channels) */
#define	DMA2_BASE0	0xC0	/* base channel 0 */
#define	DMA2_COUNT0	0xC2	/* count channel 0 */
#define	DMA2_BASE1	0xC4	/* base channel 1 */
#define	DMA2_COUNT1	0xC6	/* count channel 1 */
#define	DMA2_BASE2	0xC8	/* base channel 2 */
#define	DMA2_COUNT2	0xCA	/* count channel 2 */
#define	DMA2_BASE3	0xCC	/* base channel 3 */
#define	DMA2_COUNT3	0xCE	/* count channel 3 */
#define	DMA2_CMD	0xD0	/* command status */
#define	DMA2_REQ	0xD2	/* request register */
#define	DMA2_SMASK	0xD4	/* single mask */
#define	DMA2_MODE	0xD6	/* mode register */
#define	DMA2_FF		0xD8	/* internal flip-flop */

/* page registers */
#define	DMA_PAGE0	0x87	/* page register for channel 0 */
#define	DMA_PAGE1	0x83	/* page register for channel 0 */
#define	DMA_PAGE2	0x81	/* page register for channel 0 */
#define	DMA_PAGE3	0x82	/* page register for channel 0 */
#define	DMA_PAGE4	0x00	/* unused */
#define	DMA_PAGE5	0x8B	/* page register for channel 5 */
#define	DMA_PAGE6	0x89	/* page register for channel 6 */
#define	DMA_PAGE7	0x8F	/* page register for channel 7 */

/* command register values */
#define	DMA_ENAMEM	0x01	/* memory to memory enable */
#define	DMA_DISMEM	0x00	/* memory to memory disable */
#define	DMA_ENAHOLD0	0x02	/* channel 0 address hold enable */
#define	DMA_DISHOLD0	0x00	/* channel 0 address hold disable */
#define	DMA_ENACRTL	0x00	/* controller enable */
#define	DMA_DISCRTL	0x04	/* controller disable */
#define	DMA_NORMALTIME	0x00	/* normal timing */
#define	DMA_COMPRTIME	0x08	/* compressed timing */
#define	DMA_FIXEDPRI	0x00	/* fixed priority */
#define	DMA_ROTATEPRI	0x10	/* rotating priority */
#define	DMA_LATEWRITE	0x00	/* late write selection */
#define	DMA_EXTWRITE	0x20	/* extended write selection */
#define	DMA_DEFCMD	(DMA_ENACRTL|DMA_FIXEDPRI|DMA_EXTWRITE)

/* mode register values */
#define	DMA_CHANMASK	0x03	/* channel select mask */
#define	DMA_VERIFY	0x00	/* verify transfer */
#define	DMA_WRITE	0x04	/* write transfer */
#define	DMA_READ	0x08	/* read transfer */
#define	DMA_ENAINIT	0x00	/* auto initialization enabled */
#define	DMA_DISINIT	0x10	/* auto initialization disabled */
#define	DMA_ADDRINC	0x00	/* address increment select */
#define	DMA_ADDRDECR	0x20	/* address decrement select */
#define	DMA_DEMAND	0x00	/* demand mode select */
#define	DMA_SINGLE	0x40	/* single mode select */
#define	DMA_BLOCK	0x80	/* block mode select */
#define	DMA_CASCADE	0xC0	/* cascade mode select */

/* request register values */
#define	DMA_RESETREQ	0x00	/* reset request bit */
#define	DMA_SETREQ	0x04	/* set request bit */

/* mask register values */
#define	DMA_ENACHAN	0x00	/* channel enable */
#define	DMA_DISCHAN	0x04	/* channel disable */

/* predefined DMA channels */
#define	DMA_SDLC	1	/* synchroneous data link control channel */
#define	DMA_FLOPPY	2	/* floppy channel */
#define	DMA_CASCADE1	4	/* cascade for controller 1 */

