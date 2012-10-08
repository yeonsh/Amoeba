/*	@(#)filesvr.h	1.2	94/04/07 15:05:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef FILESVR_H
#define FILESVR_H

#include "_ARGS.h"
void add_file_server 	 _ARGS((char *name, char *report_name));
void lookup_file_servers _ARGS((char *dir));



struct filesvr {
    char 	*fsvr_name;
    capability	 fsvr_cap;
};

extern struct filesvr *filesvr_list;
extern int n_file_svrs;

#endif
