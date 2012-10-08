/*	@(#)dirsvr.h	1.1	96/02/27 10:02:39 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#ifndef DIRSVR_H
#define DIRSVR_H

void startgroup _ARGS((int min_copies, int ncpu));

void process_group_requests _ARGS((void));

int  get_sp_min_copies _ARGS((void));
int  get_write_op      _ARGS((void));
int  get_read_op       _ARGS((void));
int  get_total_member  _ARGS((void));
long get_group_seqno   _ARGS((void));

errstat send_to_group _ARGS((header *hdr, char *buf, int size));

int  dirsvr_functioning _ARGS((void));
void broadcast_time     _ARGS((void));

#endif
