/*	@(#)scsi_dma.h	1.2	94/04/06 09:31:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SCSI_DMA_H__
#define __SCSI_DMA_H__

/*
 *	L64853 LSI Logic DMA Controller
 */

typedef struct l64853	dmactlr;
struct l64853
{
    volatile uint32	l_stat_ctl;
    volatile uint32	l_addr;
    volatile uint32	l_byte_cnt;	/* top 8 bits are always 0 */
    volatile uint32	l_diag;
};

#ifndef	__bit
#define	__bit(n)		(1 << (n))
#endif

#define	__bits(a, b)		(__bit(a) | __bit(b))

/* Status Control Register */
/***************************/
#define	L64853_INT_PEND		__bit(0)	/* Read Only */
#define	L64853_ERR_PEND		__bit(1)	/* Read Only */
#define	L64853_PACK_CNT		__bits(2, 3)	/* Read Only */
#define	L64853_INT_EN		__bit(4)	/* Read/Write */
#define	L64853_FLUSH		__bit(5)	/* Write Only */
#define	L64853_DRAIN		__bit(6)	/* Read/Write */
#define	L64853_RESET		__bit(7)	/* Read/Write */
#define	L64853_WRITE		__bit(8)	/* Read/Write */
#define	L64853_EN_DMA		__bit(9)	/* Read/Write */
#define	L64853_REQ_PEND		__bit(10)	/* Read Only */
#define L64853_BYTE_ADDR	__bits(11, 12)	/* Read Only */
#define	L64853_EN_CNT		__bit(13)	/* Read/Write */
#define	L64853_TC		__bit(14)	/* Read Only */
#define	L64853_ILACC		__bit(15)	/* Read/Write */
#define L64853_RESERVED		XXXXXXXXX	/* bits 16-27, Read Only */
#define L64853_DEV_ID_MASK	0xF0000000	/* Read Only */

#define	L64853_MAX_XFER		0x01000000	/* 16 Mbyte */
/* Known chip revisions */
#define	L64853_DEV_ID1		0x80000000
#define	L64853_DEV_ID2		0x90000000
#define	L64853_DEV_ID3		0x40000000

/* Possible values for the WRITE bit */
#define	L64853_DMA_TO_MEM	L64853_WRITE
#define	L64853_DMA_FROM_MEM	0

/* Size in bytes of largest safe DMA xfer */
#define	MAX_DMA_BYTES	(1 << 16)

#endif /* __SCSI_DMA_H__ */
