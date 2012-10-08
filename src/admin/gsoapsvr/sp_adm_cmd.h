/*	@(#)sp_adm_cmd.h	1.1	96/02/27 10:02:58 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#ifndef ADM_CMD_H
#define ADM_CMD_H

void    start_adm_server _ARGS((void));
void    age_directory    _ARGS((header *hdr, char *buf, int size));
errstat remove_dir       _ARGS((objnum obj));
void    sp_age		 _ARGS((header *hdr));
void	got_touch_list   _ARGS((header *hdr, char *buf, int size));

#endif
