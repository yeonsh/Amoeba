/*	@(#)pd_preserve.c	1.4	96/02/27 11:03:28 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Code to preserve a process.  This creates a single (bullet) file
   containing the process descriptor followed by the segments; in
   theory, the file is executable again. */

/* Amoeba .h files */
#include <stdlib.h>
#include <string.h>
#include <amtools.h>
#include <module/proc.h>
#include <bullet/bullet.h>


static errstat sd_save(); /* Forward */

int
pd_preserve(filesvr, pd, pdlen, file)
	capability *filesvr;
	process_d *pd;
	int pdlen;
	capability *file; /*out*/
{
	int err;
	uint16 i;
	process_d *pdcopy;
	segment_d *sd;
	char *buf, *bufend;
	b_fsize offset;
	
	/* Because we need to make a modified copy and then need to marshal
	   the modified copy, we have to malloc two buffers.  Sigh. */
	
	/* Allocate space for the first copy (in native byte order) */
	pdcopy = (process_d *) malloc((size_t) pdlen);
	if (pdcopy == NULL)
		return STD_NOMEM;
	
	/* Allocate space for the second copy (in network byte order) */
	buf = (char *) malloc((size_t) pdlen);
	if (buf == NULL) {
		err = STD_NOMEM;
		goto cleanup2;
	}
	
	/* Copy pd to the first copy, that we will modify */
	(void) memmove((char *)pdcopy, (char *)pd, pdlen);
	
	/* Marshal the first copy to the second one */
	if ((bufend = buf_put_pd(buf, buf+pdlen, pdcopy)) == NULL) {
		err = STD_SYSERR;
		goto cleanup;
	}
	
	/* Create a file, write the second copy to it.  Don't commit
	   yet.  Later we will overwrite this with a modified copy.
	   'offset' will hold the size of the file so far. */
	offset = pdlen = bufend - buf;
	err = b_create(filesvr, buf, offset, 0, file);
	if (err != STD_OK)
		goto cleanup;
	
	/* Copy the segments to the file */
	for (i = 0, sd = PD_SD(pdcopy); err == STD_OK && i < pdcopy->pd_nseg;
								++i, ++sd) {
		if (!NULLPORT(&sd->sd_cap.cap_port) &&
				(sd->sd_type & MAP_TYPEMASK) != 0) {
			err = sd_save(file, &offset, sd);
		}
	}
	
	if (err == STD_OK) {
		/* Now marshal the modified first copy of pd again and
		   write it again to the file, this time committing. */
		if ((bufend = buf_put_pd(buf, buf+pdlen, pdcopy)) == NULL)
			err = STD_SYSERR;
		else {
			offset = pdlen;
			err = b_modify(file, (b_fsize)0, buf, offset,
							BS_COMMIT, file);
		}
	}
	
	/* If we failed, destroy the (uncommitted) file */
	if (err != STD_OK)
		(void) std_destroy(file);
	
	/* And clean up the buffers we allocated */
cleanup:
	free(buf);
cleanup2:
	free((char *)pdcopy);
	
	/* Return the error code */
	return err;
}

/* Append the segment to the file; modify the sd to reflect this */

#define BUFSIZE BS_REQBUFSZ /* Length of read buffer */

static errstat
sd_save(file, p_offset, sd)
	capability *file;
	b_fsize *p_offset;
	segment_d *sd;
{
	b_fsize segoffset, fileoffset, size;
	char *buf;
	errstat err;
	
	buf = (char *) malloc(BUFSIZE);
	if (buf == NULL)
		return STD_NOMEM;
	fileoffset = *p_offset;
	segoffset = sd->sd_offset;
	size = sd->sd_len;
	err = STD_OK;
	while (size > 0) {
		b_fsize chunk = size;

		if (chunk > BUFSIZE)
			chunk = BUFSIZE;
		if ((sd->sd_type & MAP_INPLACE) == 0) {
			err = b_read(&sd->sd_cap, segoffset,
				     buf, chunk, &chunk);
			if (err != STD_OK)
				break;
		} else {
			/* We cannot get the contents of a hardware segment
		 	 * in general, so we'll just create an empty segment.
		         */
			(void) memset((_VOIDSTAR) buf, '\0', (size_t) chunk);
		}
		err = b_modify(file, fileoffset, buf, chunk, 0, file);
		if (err != STD_OK)
			break;
		segoffset += chunk;
		fileoffset += chunk;
		size -= chunk;
	}
	if (err == STD_OK) {
		CAPZERO(&sd->sd_cap);
		sd->sd_offset = *p_offset;
		*p_offset = fileoffset;
		sd->sd_type &= ~MAP_AND_DESTROY;
	}
	free(buf);
	return err;
}
