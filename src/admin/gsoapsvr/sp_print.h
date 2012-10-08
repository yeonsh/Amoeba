/*	@(#)sp_print.h	1.1	96/02/27 10:03:01 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#ifndef SP_PRINT_H
#define SP_PRINT_H

#include "soap/soap.h"

char *cmd_name	    _ARGS((command cmd));
void print_row	    _ARGS((struct sp_row *sr, int ncols));
void sp_print_dir   _ARGS((struct sp_dir *sd));
void sp_print_table _ARGS((void));

#endif
