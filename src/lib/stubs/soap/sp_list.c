/*	@(#)sp_list.c	1.3	94/04/07 11:09:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"
#include "sp_dir.h"
#include "sp_buf.h"

extern char *strdup();

#define Failure()	{ goto failure; }

/* Add rows to an existing directory, as created by sp_makedir.  This will
 * be called repeatedly when sp_list has to do multiple transactions to get
 * all the rows of a large directory:
 */
static SP_DIR *
sp_addtodir(dd, firstrow, buff, n)
SP_DIR *dd;
int     firstrow;
char   *buff;
int     n;
{
    int16 ncols, nrows;
    int   i;
    char *p, *e, *name;

    /* Skip over the per-directory info in the buffer -- we only care
     * about adding missing rows to an existing dir:
     */
    p = buff;
    e = &buff[n];

    p = buf_get_int16(p, e, &ncols);
    p = buf_get_int16(p, e, &nrows);
    if (p == NULL || ncols != dd->dd_ncols || nrows != dd->dd_nrows) {
	Failure();
    }
    for (i = 0; i < ncols; i++) {
	p = buf_get_string(p, e, &name);
    }
    if (p == NULL) {
	Failure();
    }

    for (i = firstrow; i < nrows; i++) {
	struct sp_direct *d;
	int col;

	d = &dd->dd_rows[i];
	p = buf_get_string(p, e, &name);
	if (p == NULL) {
	    /* The buffer ran out before we got all the rows.
	     * This is normal for a large dir and will be handled by sp_list.
	     */
	    break;
	}
	d->d_name = strdup(name);
	d->d_namlen = strlen(name);
	d->d_columns = (rights_bits *) alloc(sizeof(rights_bits), ncols);
	if (d->d_name == NULL || d->d_columns == NULL) {
	    Failure();
	}
	for (col = 0; col < ncols; col++) {
	    p = buf_get_right_bits(p, e, &d->d_columns[col]);
	}
	if (p == NULL) {
	    Failure();
	}
    }
    return dd;

failure:
    sp_closedir(dd);
    return NULL;
}

static SP_DIR *
sp_makedir(dir, buff, n)
capset *dir;
char *buff;
int n;
{
    int16   ncols, nrows;
    int     col;
    char   *p, *e, *name;
    SP_DIR *dd = NULL;

    if ((dd = (SP_DIR *) alloc(sizeof(SP_DIR), 1)) == NULL) {
	Failure();
    }
    p = buff;
    e = &buff[n];

    p = buf_get_int16(p, e, &ncols);
    p = buf_get_int16(p, e, &nrows);
    if (p == NULL || cs_copy(&dd->dd_capset, dir) == 0) {
	Failure();
    }
    dd->dd_ncols = ncols;
    dd->dd_nrows = nrows;
    dd->dd_colnames = (char **) alloc(sizeof(char *), ncols);
    if (dd->dd_colnames == NULL) {
	Failure();
    }
    for (col = 0; col < ncols; col++) {
	p = buf_get_string(p, e, &name);
	if (p == NULL || (dd->dd_colnames[col] = strdup(name)) == NULL) {
	    Failure();
	}
    }

    if (nrows > 0) {
	dd->dd_rows = (struct sp_direct*)alloc(sizeof(struct sp_direct),nrows);
	if (dd->dd_rows == NULL) {
	    Failure();
	}
	dd = sp_addtodir(dd, 0, buff, n);
    }
    return dd;

failure:
    if (dd != NULL) {
	sp_closedir(dd);
    }
    return NULL;
}

errstat
sp_list(dir, ddp)
capset *dir;
SP_DIR **ddp;
{
    union sp_buf *buf;
    header  hdr;
    SP_DIR *dd = NULL;
    bufsize retsize;
    uint16  next_row;
    errstat err;

    if ((buf = sp_getbuf()) == NULL) {
	return STD_NOSPACE;
    }

    hdr.h_extra = 0;	/* start listing at row 0 */
    err = sp_mktrans(SP_NTRY, dir, &hdr, SP_LIST,
		     NILBUF, (bufsize) 0,
		     buf->sp_buffer, (bufsize) sizeof(buf->sp_buffer));
    retsize = hdr.h_size;
    next_row = hdr.h_extra;

    if (err == STD_OK) {
	dd = sp_makedir(dir, buf->sp_buffer, (int) retsize);

	while ((dd != NULL) &&
	       (dd->dd_nrows > 0) &&
	       (dd->dd_rows[dd->dd_nrows - 1].d_name == '\0') /* ? */ &&
	       (err == STD_OK) &&
	       (next_row != SP_NOMOREROWS))
	{
	    uint16 first_row = next_row;

	    /* There are more rows that didn't fit in the buffer.
	     * Get them and add them to the dir structure:
	     */
	    hdr.h_extra = first_row;
	    err = sp_mktrans(SP_NTRY, dir, &hdr, SP_LIST,
			     NILBUF, (bufsize) 0,
			     buf->sp_buffer, (bufsize) sizeof(buf->sp_buffer));
	    retsize = hdr.h_size;
	    next_row = hdr.h_extra;

	    if (next_row != SP_NOMOREROWS && next_row <= first_row) {
		/* sanity check */
		err = STD_SYSERR;
	    }

	    if (err == STD_OK) {
		dd = sp_addtodir(dd, (int)first_row,
				 buf->sp_buffer, (int) retsize);
	    }
	}
    }

    /* return buffer to buffer pool */
    sp_putbuf(buf);

    if (err == STD_OK) {
	if (dd == NULL) {
	    err = STD_NOSPACE;
	}
	*ddp = dd;
    }

    return err;
}
