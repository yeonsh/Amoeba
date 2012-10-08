/*	@(#)offset.c	1.2	94/04/06 09:37:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: offset.c -- part of the process of defining constants for assembly
 * files that want to know the offset of fields within structures. For example,
 * if I had the following structure:
 *	struct foo {
 *	    int f_one, f_two, f_three;
 *          int f_four[ 4 ];
 *	};
 * and wanted to make it visible to assembly, I would say here:
 *	struct foo f;
 *	MKOFF( f, f_one, F_ONE );
 *	MKOFF( f, f_two, F_TWO );
 *	MKOFF( f, f_three, F_THREE );
 *	MKOFFARR( f, f_four, F_FOUR );
 *	MKSIZEOF( struct foo, FOO );
 * and an "offset.h" would be generated that would say:
 *	#define F_ONE 0
 *	#define F_TWO 4
 *	#define F_THREE 8
 *	#define F_FOUR 12
 *	#define SIZEOF_FOO 28
 * which assembly files can then feel free to depend on.
 *
 * Author: Philip Shafer <phil@procyon.ics.Hawaii.Edu>
 *	   (Univerity of Hawai'i, Manoa) November 1990
 */

#include <stdio.h>
#include <stdlib.h>
#include "amoeba.h"

#ifndef CONCAT
#ifdef __STDC__
#define CONCAT( a,b )	a##b
#else
#define CONCAT( a,b )	a/**/b
#endif	/* __STDC__ */
#endif	/* CONCAT */

#define DIFFERENCE( a,b )	( int )( ( ( char * )( b ) ) - ( ( char * )( a ) ) )
#define OFFSET( s,f )	DIFFERENCE( &s,&s.f )
#define MKOFF( s,f,d )	int d = OFFSET( s,f )
#define OFFSETARR( s,f )	DIFFERENCE( &s,s.f )
#define MKOFFARR( s,f,d )	int d = OFFSETARR( s,f )
#define MKSIZEOF( s,n )	int CONCAT( SIZEOF_,n ) = sizeof( s )

MKSIZEOF( int,INT );

#include "machdep.h"
#include "openprom.h"

struct openboot prom_device;
MKOFF( prom_device,op_magic,PD_MAGIC );
MKOFF( prom_device,op_romvec_version,PD_ROMVECVERSION );
MKOFF( prom_device,op_plugin_version,PD_ARCHVERSION );
MKOFF( prom_device,op_mon_id,PD_MONITORID );
MKOFF( prom_device,v1_physmem,PD_V1PHYSMEM );
MKOFF( prom_device,v1_virtmem,PD_V1VIRTMEM );
MKOFF( prom_device,v1_availmem,PD_V1AVAILMEM );
MKOFF( prom_device,op_config_ops,PD_CONFIG );
MKOFF( prom_device,v1_bootcmd,PD_V1BOOTCMD );
MKOFF( prom_device,v1_open,PD_V1OPEN );
MKOFF( prom_device,v1_close,PD_V1CLOSE );
MKOFF( prom_device,v1_blkread,PD_V1BLKREAD );
MKOFF( prom_device,v1_blkwrite,PD_V1BLKWRITE );
MKOFF( prom_device,v1_pxmit,PD_V1PXMIT );
MKOFF( prom_device,v1_precv,PD_V1PRECV );
MKOFF( prom_device,v1_bread,PD_V1BREAD );
MKOFF( prom_device,v1_bwrite,PD_V1BWRITE );
MKOFF( prom_device,v1_seek,PD_V1SEEK );
MKOFF( prom_device,v1_input,PD_V1INPUT );
MKOFF( prom_device,v1_output,PD_V1OUTPUT );
MKOFF( prom_device,v1_getc,PD_V1GETC );
MKOFF( prom_device,v1_putc,PD_V1PUTC );
MKOFF( prom_device,v1_getp,PD_V1GETP );
MKOFF( prom_device,v1_putp,PD_V1PUTP );
MKOFF( prom_device,v1_putstr,PD_V1PUTSTR );
MKOFF( prom_device,op_boot,PD_REBOOT );
MKOFF( prom_device,v1_printf,PD_V1PRINTF );
MKOFF( prom_device,op_enter,PD_ABORT );
MKOFF( prom_device,op_milliseconds,PD_CLOCK );
MKOFF( prom_device,op_exit,PD_EXIT );
MKOFF( prom_device,op_callback,PD_VECTOR );
MKOFF( prom_device,op_interpret,PD_FORTH );
MKOFF( prom_device,v1_parms,PD_PARMS );
MKOFF( prom_device,v1_eaddr,PD_EADDR );
MKOFF( prom_device,op_bootpath,PD_BOOTPATH );
MKOFF( prom_device,op_bootargs,PD_BOOTARGS );
MKOFF( prom_device,op_stdin,PD_STDIN );
MKOFF( prom_device,op_stdout,PD_STDOUT );
MKOFF( prom_device,op_phandle,PD_PHANDLE );
MKOFF( prom_device,op_alloc,PD_ALLOC );
MKOFF( prom_device,op_free,PD_FREE );
MKOFF( prom_device,op_map,PD_MAP );
MKOFF( prom_device,op_unmap,PD_UNMAP );
MKOFF( prom_device,op_open,PD_OPEN );
MKOFF( prom_device,op_close,PD_CLOSE );
MKOFF( prom_device,op_read,PD_READ );
MKOFF( prom_device,op_write,PD_WRITE );
MKOFF( prom_device,op_seek,PD_SEEK );
MKOFF( prom_device,op_chain,PD_CHAIN );
MKOFF( prom_device,op_release,PD_RELEASE );
MKOFFARR( prom_device,op_reserved,PD_RESERVED );
MKOFF( prom_device,v2_setseg,PD_SETSEG );
MKOFF( prom_device,op_startcpu,PD_STARTCPU );
MKOFF( prom_device,op_stopcpu,PD_STOPCPU );
MKOFF( prom_device,op_idlecpu,PD_IDLECPU );
MKOFF( prom_device,op_resumecpu,PD_RESUMECPU );

struct prom_bootparms prom_bootparms;
MKOFFARR( prom_bootparms,pb_argv,PB_ARGV );
MKOFFARR( prom_bootparms,pb_space,PB_SPACE );
MKOFFARR( prom_bootparms,pb_devname,PB_DEVNAME );
MKOFF( prom_bootparms,pb_cntrl,PB_CNTRL );
MKOFF( prom_bootparms,pb_unit,PB_UNIT );
MKOFF( prom_bootparms,pb_part,PB_PART );
MKOFF( prom_bootparms,pb_file,PB_FILE );
MKOFF( prom_bootparms,pb_iodev,PB_IODEV );

struct prom_iodev prom_iodev;
MKOFFARR( prom_iodev,pi_devname,PI_DEVNAME );
MKOFF( prom_iodev,pi_probe,PI_PROBE );
MKOFF( prom_iodev,pi_boot,PI_BOOT );
MKOFF( prom_iodev,pi_open,PI_OPEN );
MKOFF( prom_iodev,pi_close,PI_CLOSE );
MKOFF( prom_iodev,pi_strat,PI_STRAT );
MKOFF( prom_iodev,pi_desc,PI_DESC );

struct prom_memory prom_memory;
MKOFF( prom_memory,pm_next,PM_NEXT );
MKOFF( prom_memory,pm_addr,PM_ADDR );
MKOFF( prom_memory,pm_len,PM_LEN );

#include "promid_sun4.h"

struct mach_info mach_info;
MKOFF( mach_info,mi_id,MI_ID );
MKOFF( mach_info,mi_nwindows,MI_NWINDOWS );
MKOFF( mach_info,mi_contexts,MI_CONTEXTS );
MKOFF( mach_info,mi_pmegs,MI_PMEGS );
MKSIZEOF( struct mach_info,MACH_INFO );

#include "fault.h"

struct stack_frame stack_frame;
MKOFFARR( stack_frame,sf_local,SF_LOCAL );
MKOFFARR( stack_frame,sf_in,SF_IN );
MKOFF( stack_frame,sf_aggregate,SF_AGGREGATE );
MKOFFARR( stack_frame,sf_regsave,SF_REGSAVE );
MKOFFARR( stack_frame,sf_addarg,SF_ADDARG );
MKSIZEOF( struct stack_frame,STACK_FRAME );

struct thread_ustate local_ustate;	/* already a typedef */
MKOFF( local_ustate,tu_sf,TU_SF );
MKOFFARR( local_ustate,tu_global,TU_GLOBAL );
MKOFFARR( local_ustate,tu_local,TU_LOCAL );
MKOFF( local_ustate,tu_fsr,TU_FSR );
MKOFF( local_ustate,tu_qsize,TU_QSIZE );
MKOFFARR( local_ustate,tu_float,TU_FLOAT );
MKOFFARR( local_ustate,tu_fqueue,TU_FQUEUE );
MKSIZEOF( struct thread_ustate,THREAD_USTATE );
