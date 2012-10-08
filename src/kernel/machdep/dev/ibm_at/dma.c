/*	@(#)dma.c	1.5	94/04/06 09:18:17 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * dma.c
 *
 * The PC/AT can perform DMA operations by using the 8237 DMA chip.
 * Note that the chip isn't capable of doing DMA across 64 Kb boundaries.
 *
 * Author:
 *      Leendert van Doorn
 */
#include <bool.h>
#include <assert.h>
INIT_ASSERT
#include <amoeba.h>
#include <machdep.h>
#include <cmdreg.h>
#include <stderr.h>
#include "i386_proto.h"

#include "dma.h"

#ifndef DMA_DEBUG
#define	DMA_DEBUG	0
#endif

/*
 * Current debug level
 */
#ifndef NDEBUG
static int dma_debug;
#endif

/*
 * Convert channel numbers to base channel registers, page registers,
 * and channel count I/O ports.
 */
static int dma_base[8] = {
    DMA1_BASE0, DMA1_BASE1, DMA1_BASE2, DMA1_BASE3,
    DMA2_BASE0, DMA2_BASE1, DMA2_BASE2, DMA2_BASE3,
};
static int dma_pages[8] = {
    DMA_PAGE0, DMA_PAGE1, DMA_PAGE2, DMA_PAGE3,
    DMA_PAGE4, DMA_PAGE5, DMA_PAGE6, DMA_PAGE7,
};
static int dma_count[8] = {
    DMA1_COUNT0, DMA1_COUNT1, DMA1_COUNT2, DMA1_COUNT3,
    DMA2_COUNT0, DMA2_COUNT1, DMA2_COUNT2, DMA2_COUNT3,
};
static int dma_inuse[8];

int dma_setup8();
int dma_setup16();

/*
 * Initialize DMA driver
 */
void
dma_init()
{
#ifndef NDEBUG
    if ((dma_debug = kernel_option("dma")) == 0)
	dma_debug = DMA_DEBUG;
#endif
    /* BIOS has already properly initialized the two DMA chips */
}

/*
 * Set up a DMA channel as cascade, and enable it. The cascade
 * mode allows other chips (like the LANCE) to do DMA while still
 * using the 8237 priority mechanism.
 */
void
dma_cascade(channel)
    int channel;
{
    assert(channel < 8);
    assert(!dma_inuse[channel]);
    dma_inuse[channel] = TRUE;
#ifndef NDEBUG
    if (dma_debug > 0)
	printf("dma_cascade(channel=%d)\n", channel);
#endif
    if (DMA_CHIP1(channel)) {
	out_byte(DMA1_MODE, DMA_CASCADE | channel);
	out_byte(DMA1_SMASK, DMA_ENACHAN | channel);
    } else if (DMA_CHIP2(channel)) {
	out_byte(DMA2_MODE, DMA_CASCADE | ((channel - 4) & 03));
	out_byte(DMA2_SMASK, DMA_ENACHAN | ((channel - 4) & 03));
    }
}

/*
 * Set up a DMA channel
 */
int
dma_setup(mode, channel, addr, count)
    int mode;			/* DMA operation mode */
    int channel;		/* DMA channel */
    char *addr;			/* buffer pointer */
    int count;			/* number of bytes to move */
{
    int error;

    assert(channel < 8);
    assert(!dma_inuse[channel]);
    dma_inuse[channel] = TRUE;
    if (DMA_CHIP1(channel))
	error =  dma_setup8(mode, channel, addr, count);
    else if (DMA_CHIP2(channel))
	error = dma_setup16(mode, channel, addr, count);
    if (error) dma_inuse[channel] = FALSE;
    return error;
}

/*
 * Set up the first 8237 controller
 * (which corresponds to the four 8-bit channels).
 */
int
dma_setup8(mode, channel, addr, count)
    int mode;			/* DMA operation mode */
    int channel;		/* DMA channel */
    char *addr;			/* buffer pointer */
    int count;			/* number of bytes to move */
{
    assert(addr);
    if (count <= 0) return 1;
#ifndef NDEBUG
    if (dma_debug > 0)
	printf("dma_setup8(mode=%x, channel=%d, address=%x, count=%d)\n",
	    mode, channel, addr, count);
#endif
    /*
     * The 8237 chip can only move 65536 bytes (in 8 bit mode) or
     * 131072 bytes (in 16 bit mode). To enable the chip to write
     * data in the whole 16 Mb address space, a special chip controls
     * the highest 8 bits of address line connected to the DMA chip;
     * The consequence of this design is that memory is divided into 64Kb
     * or 128Kb memory chunks, and memory cannot be moved accross these
     * chunk boundaries since the chip that controls the top address lines
     * is not updated. Me think, this is a design flaw.
     */
    if (((((int)addr + count-1) >> 16) & 0xFF) != (((int)addr >> 16) & 0xFF)) {
	printf("dma_setup8: DMA crosses 64K boundary\n");
	return 1;
    }
    out_byte(DMA1_SMASK, DMA_DISCHAN | channel);
    out_byte(DMA1_FF, mode | channel);
    out_byte(DMA1_MODE, mode | channel);
    out_byte(dma_base[channel], (int)addr & 0xFF);
    out_byte(dma_base[channel], ((int)addr >> 8) & 0xFF);
    out_byte(dma_pages[channel], ((int)addr >> 16) & 0xFF);
    out_byte(dma_count[channel], (count - 1) & 0xFF);
    out_byte(dma_count[channel], ((count - 1) >> 8) & 0xFF);
    return 0;
}

/*
 * Set up the second 8237 controller
 * (which corresponds to the four 16-bit channels).
 */
int
dma_setup16(mode, channel, addr, count)
    int mode;			/* DMA operation mode */
    int channel;		/* DMA channel */
    char *addr;			/* buffer pointer */
    int count;			/* number of bytes to move */
{
    assert(addr);
    if (count <= 0) return 1;
#ifndef NDEBUG
    if (dma_debug > 0)
	printf("dma_setup16(mode=%x, channel=%d, address=%x, count=%d)\n",
	    mode, channel, addr, count);
#endif
    if (((int)addr & 1) == 1) {
	printf("dma_setup16: DMA can't transfer from odd boundary\n");
	return 1;
    }
    if (((((int)addr + count-1) >> 16) & 0xFF) != (((int)addr >> 16) & 0xFF)) {
	printf("dma_setup16: DMA crosses 64K boundary\n");
	return 1;
    }
    out_byte(DMA2_SMASK, DMA_DISCHAN | ((channel - 4) & 03));
    out_byte(DMA2_FF, mode | (channel - 4));
    out_byte(DMA2_MODE, mode | (channel - 4));
    out_byte(dma_base[channel], (int)addr & 0xFF);
    out_byte(dma_base[channel], ((int)addr >> 8) & 0xFF);
    out_byte(dma_pages[channel], ((int)addr >> 16) & 0xFF);
    out_byte(dma_count[channel], (count - 1) & 0xFF);
    out_byte(dma_count[channel], ((count - 1) >> 8) & 0xFF);
    return 0;
}

/*
 * Start a DMA request on a given channel
 */
void
dma_start(channel)
    int channel;
{
    assert(channel < 8);
    assert(dma_inuse[channel]);
    if (DMA_CHIP1(channel))
	out_byte(DMA1_SMASK, DMA_ENACHAN | channel);
    else if (DMA_CHIP2(channel))
	out_byte(DMA2_SMASK, DMA_ENACHAN | ((channel - 4) & 03));
}

/*
 * Stop DMA transfer by disabling DMA channel for
 * all further DMA operations.
 */
void
dma_done(channel)
    int channel;
{
    assert(channel < 8);
    assert(dma_inuse[channel]);
    if (DMA_CHIP1(channel))
	out_byte(DMA1_SMASK, DMA_DISCHAN | channel);
    else if (DMA_CHIP2(channel))
	out_byte(DMA2_SMASK, DMA_DISCHAN | (channel - 4) & 03);
    dma_inuse[channel] = FALSE;
}
