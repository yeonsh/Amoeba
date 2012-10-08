/*	@(#)f_info.h	1.2	94/04/07 14:50:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */


/* used during parsing, keeps file read from and current line */

struct f_info {
    char  *f_file;
    int	   f_line;
};

