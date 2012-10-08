/*	@(#)proto.h	1.4	96/02/27 10:12:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#if !defined(__PROTO_H__)
#define __PROTO_H__

#include "amoeba.h"
#include "module/disk.h"
#include "vdisk/disklabel.h"
#include "dsktab.h"
#include "vdisk/vdisk.h"

/* Menu functions */
void menu_display_pdisks _ARGS(( void ));
void menu_display_vdisks _ARGS(( void ));
void display_partn_list _ARGS(( void ));
void label_pdisks   _ARGS(( void ));
void label_partns   _ARGS(( void ));
void merge_vdisks   _ARGS(( void ));
void unmerge_vdisks _ARGS(( void ));
void noop	    _ARGS(( void ));

/* Input functions */
int  confirm _ARGS(( char * question ));
void getfield16 _ARGS(( char * s, int16 * number ));
void getfield32 _ARGS(( char * s, int32 * number ));
void getline _ARGS(( char * begin, char * end ));
int  getnumber _ARGS(( char * ));
void pause _ARGS(( void ));

/* from disklabel.c */
errstat writeblock _ARGS(( capability *, char *, disk_addr, char * ));
errstat write_amlabel _ARGS(( capability *, char *, disk_addr, disklabel * ));
errstat read_amlabel _ARGS(( capability *, char *, disk_addr, disklabel * ));
int am_partn_label _ARGS(( disklabel * ));
disk_addr total_blocks _ARGS(( part_tab * ));

/* odds and ends */
void make_partn_list _ARGS(( void ));
void make_vdisk_list _ARGS(( void ));
void renumber_vdisks _ARGS(( void ));
vdisktab * find_entry _ARGS (( vdisktab *, objnum ));
void display_pdisks _ARGS(( void ));
void display_vdisks _ARGS(( void ));
vdisktab * new_entry _ARGS(( vdisktab *, objnum, part_list *, int ));

/* From the amoeba library */
uint16	checksum _ARGS(( uint16 * data ));

#endif /* __PROTO_H__ */
