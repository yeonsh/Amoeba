/*	@(#)nvdev.h	1.1	96/02/27 10:30:35 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Prototypes for the NVRAM service functions.
 *
 * Author: Gregory J. Sharp, Jan 1995
 */

#include "amoeba.h"
#include "disk/disk.h"
#include "disk/conf.h"


#if !defined(COLDSTART)


void nv_add_RAM _ARGS(( vir_bytes nvp, vir_bytes size, vir_bytes skip ));
errstat nv_add_disk _ARGS(( int ctlr, int unit ));
errstat nv_read _ARGS(( long devaddr, int unit, disk_addr strt_blk,
				disk_addr blk_cnt, char * buf ));
errstat nv_write _ARGS(( long devaddr, int unit, disk_addr strt_blk,
				disk_addr blk_cnt, char * buf ));
errstat nv_ctlr_init _ARGS(( long devaddr, int unit ));
errstat nv_unit_init _ARGS(( long devaddr, int unit ));
errstat nv_unit_type _ARGS(( long devaddr, int unit ));
errstat nv_capacity _ARGS(( long devaddr, int unit, int32 * lastblk,
				int32 * blksz ));
errstat nv_control _ARGS(( long devaddr, int unit, int cmd, bufptr buf,
				bufsize size ));
errstat nv_geometry _ARGS(( long devaddr, int unit, geometry * geom ));

#endif /* COLDSTART */
