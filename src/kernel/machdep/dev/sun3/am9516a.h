/*	@(#)am9516a.h	1.3	94/04/06 09:24:48 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __AM9516A_H__
#define __AM9516A_H__

/*
** Am9516A UDC (Universal DMA Controller) Registers.
**
** 	The internal registers of this device are addressed by feeding
**	the address of a udc_cmdblk (in main memory) to the udc's two
**	external registers.  The chip then reads in the command block and
**	executes the command described therein.
*/

typedef struct am_9516a am_9516a;
struct am_9516a
{
    volatile uint16	udc_rdata;
    volatile uint16	udc_raddr; /* Least significant byte is not used !? */
};

/* 
** For a dma transfer, the udc registers are loaded from a command block
** in memory pointed to by the chain address register.
**  NB: this struct must always be even byte aligned!
**
** d_regsel:	selects udc register to be loaded
** d_haddr:	high word of main memory source/target address
** d_laddr:	low word of main memory source/target address
** d_cnt:	number of *words* to transfer
** d_hcmr:	high word of channel mode register
** d_lcmr:	low word of channel mode register
*/

typedef struct dma_cmdblk	dma_cmdblk;
struct dma_cmdblk
{
    uint16	d_regsel;
    uint16	d_haddr;
    uint16	d_laddr;
    uint16	d_cnt;
    uint16	d_hcmr;
    uint16	d_lcmr;
};


/*
** dma_ctlr defines the information necessary to manage the DMA controller.
** d_mtx:	a lock to provide single access
** d_cmd:	a command block to hold the data to set up the DMA.
*/

typedef struct dma_ctlr	dma_ctlr;
struct dma_ctlr
{
    mutex	d_mtx;
    dma_cmdblk	d_cmd;
};


#define bit(n)	(1 << (n))

/* Bits in Master Mode Register */

#define MM_CHIP_EN	bit(0)
#define MM_INTERL_EN	bit(1)
#define MM_WAIT_EN	bit(2)
#define MM_NO_VEC	bit(3)

/* initial setting for the master mode register */

#define MM_INIT		(MM_NO_VEC | MM_WAIT_EN | MM_CHIP_EN)

/*
 * pointer Register
 * LSB is not used!!
 * use these codes for the pointer register to access the desired register
 * SCSI control logic only uses channel 1
 */

#define PTR_MASTERMODE	0x38	/* master mode register */
#define PTR_COMMAND	0x2e	/* command register (write only) */
#define PTR_STATUS	0x2e	/* status register (read only) */
#define PTR_CHAR_HIGH	0x26	/* chain addr reg, high word */
#define PTR_CHAR_LOW	0x22	/* chain addr reg, low word */
#define PTR_CARA_HIGH	0x1a	/* current addr reg A, high word */
#define PTR_CARA_LOW	0x0a	/* current addr reg A, low word */
#define PTR_CARB_HIGH	0x12	/* current addr reg B, high word */
#define PTR_CARB_LOW	0x02	/* current addr reg B, low word */
#define PTR_CMR_HIGH	0x56	/* channel mode reg, high word */
#define PTR_CMR_LOW	0x52	/* channel mode reg, low word */
#define PTR_COP_COUNT	0x32	/* number of words to transfer */

/* Command Register */

#define	CR_RESET	0x00

/*
** For the sun 3 the 9516 sould be programmed in the "Flyby transaction,
** single opeartion mode"
*/

/*
** Select the registers for the transfer - send to scsi or receive from scsi
*/
#define UDC_RSEL_RECEIVE	0x0182
#define UDC_RSEL_SEND		0x0282

/*
** The following are set in the channel mode register to select the
** dma direction
*/
#define UDC_CMR_HIGH		0x0000	/* high word */
#define UDC_CMR_LSEND		0x00c2	/* low word when sending to scsi */
#define UDC_CMR_LRECV		0x00d2	/* low word when receiving from scsi */

/*
** The address control information goes in the low byte of the high word
** of the dma address.
** Data is in system memory and increment address after each word is copied.
*/
#define UDC_ADDR_CTRL		0x40

/* udc commands */
#define UDC_CMD_START_CHAIN	0xa0	/* start command chaining */
#define UDC_CMD_C1ENABLE	0x32	/* channel 1 interrupt enable */
#define UDC_CMD_RESET		0x00	/* reset command */

/* the time one must wait between register access of the udc chip */
#define	UDC_ACCESS_DELAY	1	/* 1 microsecond */

#endif /* __AM9516A_H__ */
