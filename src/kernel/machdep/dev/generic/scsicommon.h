/*	@(#)scsicommon.h	1.1	96/02/27 13:49:45 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Prototypes for the external interface SCSI routines common to all
 * device types.
 */

#ifndef	__GENERIC_SCSICOMMON_H__
#define	__GENERIC_SCSICOMMON_H__

errstat scsi_ctlr_init _ARGS(( long devaddr, int unit ));
errstat scsi_info _ARGS(( long devaddr, int unit, char * start, char * end ));
errstat scsi_status _ARGS(( long devaddr, int unit, char * start, char * end ));
int	scsi_unit_type _ARGS(( long devaddr, int unit ));

#endif /* __GENERIC_SCSICOMMON_H__ */
