/*	@(#)scsi_dma.h	1.2	94/04/06 09:38:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SCSI_DMA_H__
#define __SCSI_DMA_H__

/*
 *	L64854 LSI Logic DMA Controller
 */

typedef struct l64854	dmactlr;
struct l64854
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
#define	L64854_INT_PEND		__bit(0)	/* Read Only */
#define	L64854_ERR_PEND		__bit(1)	/* Read Only */
#define	L64854_DRAINING		__bits(2, 3)	/* Read Only */
#define	L64854_INT_EN		__bit(4)	/* Read/Write */
#define	L64854_INVALIDATE	__bit(5)	/* Write Only */
#define	L64854_SLAVE_ERR	__bit(6)	/* Read/Write 1 */
#define	L64854_RESET		__bit(7)	/* Read/Write */
#define	L64854_WRITE		__bit(8)	/* Read/Write */
#define		L64854_DMA_TO_MEM	L64854_WRITE
#define		L64854_DMA_FROM_MEM	0
#define	L64854_EN_DMA		__bit(9)	/* Read/Write */
#define	L64854_UNUSED1		__bit(10)	/* Read Only */
#define L64854_UNUSED2		__bits(11, 12)	/* Read Only */
#define	L64854_EN_CNT		__bit(13)	/* Read/Write */
#define	L64854_TC		__bit(14)	/* Read/Write 1 */
#define	L64854_UNUSED3		__bit(15)	/* Read Only */
#define	L64854_DIS_CSR_DRAIN	__bit(16)	/* Read/Write */
#define	L64854_DIS_SCSI_DRAIN	__bit(17)	/* Read/Write */
#define	L64854_BURST_SIZE	__bits(18,19)	/* Read/Write */
#define		L64854_BURST1		__bit(19)
#define		L64854_BURST4		0
#define		L64854_BURST8		__bit(18)
#define L64854_DIAG		__bit(20)	/* Read/Write */
#define L64854_TWO_CYCLE	__bit(21)	/* Read/Write */
#define	L64854_FASTER		__bit(22)	/* Read/Write */
#define L64854_TC_INT_DISABLE	__bit(23)	/* Read/Write */
#define L64854_ENABLE_NEXT	__bit(24)	/* Read/Write */
#define L64854_DMA_ON		__bit(25)	/* Read Only */
#define L64854_ADDR_LOADED	__bit(26)	/* Read Only */
#define L64854_NEXT_ADDR_LOADED	__bit(27)	/* Read Only */
#define L64854_DEV_ID_MASK	0xF0000000	/* Read Only */

#define	L64854_MAX_XFER		0x01000000	/* 16 Mbyte */

/* Known chip revisions */
#define	L64854_DEV_ID1		0xA0000000

/* Size in bytes of largest safe DMA xfer */
#define	MAX_DMA_BYTES	(1 << 16)

#endif /* __SCSI_DMA_H__ */
