/*	@(#)scsi_dmaprot.h	1.1	94/04/06 16:45:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SCSI_PROTO_H__
#define	__SCSI_PROTO_H__

/*
 * Prototypes for standard scsi_dma routines.
 */
void	scsi_dma_enable_intr	_ARGS((dmactlr *));
int	scsi_dma_error		_ARGS((dmactlr *));
void	scsi_dma_go		_ARGS((dmactlr *));
void	scsi_dma_stop		_ARGS((dmactlr *));
void	scsi_dma_reset		_ARGS((dmactlr *));
void	scsi_dma_setup		_ARGS((dmactlr *, int, uint32, uint32));

#endif /* __SCSI_PROTO_H__ */
