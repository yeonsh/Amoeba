/*	@(#)scsi_sun4.h	1.4	96/02/27 10:31:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SCSI_SUN4_H__
#define __SCSI_SUN4_H__

/*
 * This file contains the information/data structures necessary for the
 * sun4c & sun4m SCSI driver.
 *
 * Author: Gregory J. Sharp, September 1991
 */

#define SUN_NUM_SCSI_CONTROLLERS	8

typedef	struct scsi_sun4info	scsi_sun4info;

struct scsi_sun4info
{
    vir_bytes	s_scsi_ctlr;	/* Virtual address of SCSI device */
    vir_bytes	s_dma_ctlr;	/* Virtual address of DMA device */
    unsigned	s_ivec;		/* Interrupt vector for the SCSI device */
    int		s_sel_timeout;	/* Value for select timeout register */
    uint32	s_clockfreq;	/* Clock frequency in MHz */
    uint8	s_clk_cnv;	/* Clock conversion factor for SCSI chip */
    uint8	s_initiator_id;	/* Initiator id of the SCSI controller */
};

#endif /* !__SCSI_SUN4_H__ */
