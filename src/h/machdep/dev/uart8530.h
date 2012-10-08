/*	@(#)uart8530.h	1.2	96/02/27 10:31:27 */
#ifndef __UART8530_H__
#define	__UART8530_H__

/* Some defines for the Zilog 8530 DUART chip */

#ifndef _bit
#define	_bit(n)		(1 << (n))
#endif


/* magic constants & macros for internal baud generator */
#define HZ			4915200L
#define B(x)			(HZ / 32 / (x) - 2)

/* Register selector */
#define R(x)			(x)

struct z8530
{
    volatile unsigned char	csr;	/* Control/status register B */
    char			dummy1;
    volatile unsigned char	dr;	/* data register B */
    char			dummy2;
};


/* Read register 0 */
#define	RR0_RxRDY		_bit(0)
#define	RR0_TxRDY		_bit(2)

/* Read register 1 */
#define	RR1_FRAME_ERR		_bit(6)
#define	RR1_OVERRUN_ERR		_bit(5)
#define	RR1_PARITY_ERR		_bit(4)
#define	RR1_ALL_SENT		_bit(0)

/* Read register 2B - interrupt status */
#define	RR2_B_XMIT		0x0
#define	RR2_B_EXT_STATUS	0x1
#define	RR2_B_RCV		0x2
#define	RR2_B_SRC		0x3	/* Special receive condition */
#define	RR2_A_XMIT		0x4
#define	RR2_A_EXT_STATUS	0x5
#define	RR2_A_RCV		0x6
#define	RR2_A_SRC		0x7	/* Special receive condition */

/* Read register 3A - interrupt status */
#define	RR3_B_EXT_STATUS	_bit(0)
#define	RR3_B_XMIT		_bit(1)
#define	RR3_B_RCV		_bit(2)
#define	RR3_A_EXT_STATUS	_bit(3)
#define	RR3_A_XMIT		_bit(4)
#define	RR3_A_RCV		_bit(5)

#endif /* __UART8530_H__ */
