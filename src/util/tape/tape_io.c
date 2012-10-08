/*	@(#)tape_io.c	1.3	96/02/27 13:17:32 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** This module is for UNIX-like systems
**
** tape_io.c, a program module which holds the tape r/w routines.
** tape_read, reads data from tape.
** tape_write, writes data to tape.
**
** peterb@cwi.nl, 120190
**
*/
#include <stdio.h>
#include <amtools.h>
#include <module/tape.h>
#include "commands.h"

/*
** global variables.
**
*/
extern uint8		trans_buf[T_REQBUFSZ];
extern errstat		err;
extern bufsize		actual;
extern long		in, out;
extern long		count;
extern int		record_size;
extern int		verbose;
extern capability	tape_cap;
extern char *		cmd;
extern char *		commands[];

/*
** mtape_read, read data from tape and write it to stdout.
**
*/
void
mtape_read()
{
	while (count == -1 || count > 0) {
		err = tape_read(&tape_cap, (bufptr)trans_buf, 
				     record_size, &actual);
		if (actual != 0) /* we did read something */
		    in++;
		if (err != STD_OK) {
			if (verbose)
				write_verbose();

			if (err == TAPE_EOF)
				return;

			fprintf(stderr, 
				"%s: tape_read error: %s (actual read 0x%x)\n", 
				commands[READ], tape_why(err), actual);
			exit(1);
		}
	
		if (write(fileno(stdout), trans_buf, actual) != actual) {
			if (verbose)
				write_verbose();
			perror("write - stdout");
			exit(1);
		}
		out++;
		if (count != -1)
			count--;
	}
}


/*
** mtape_write, write bytes from stdin to tape.
**
*/
void
mtape_write()
{
	bufptr tmp_buf;
	int tmp_count, num_bytes;

	while (count == -1 || count > 0) {
		tmp_count = record_size;
		tmp_buf   = (bufptr)trans_buf;
		do {
			num_bytes = read(fileno(stdin), tmp_buf, tmp_count);
			if (num_bytes == -1) {
				if (verbose)
					write_verbose();
				perror("read - stdin");
				exit(1);
			}
			if (num_bytes == 0)		/* Got an EOF */
				break;
			tmp_buf   += num_bytes;
			tmp_count -= num_bytes;
		} while (tmp_count > 0);
		in++;

		if (record_size - tmp_count > 0) {
			/* must pad to full blocks? */
			while (tmp_count > 0) {
			    *tmp_buf++ = '\0';
			    tmp_count--;
			}
			err = tape_write(&tape_cap, (bufptr)trans_buf, 
					record_size - tmp_count, &actual);
			if (actual != 0)
			    out++;
			if (err != STD_OK) {
				if (verbose)
					write_verbose();
				fprintf(stderr, "%s: tape_write error: %s (actual written 0x%x)\n", 
					commands[WRITE], tape_why(err), actual);
				exit(1);
			}
		}
		if (count != -1)
			count--;
		if (tmp_count != 0)
			count = 0;
	}

	/*
	** Write an eof marker onto the tape...
	**
	*/
	if (verbose)
		write_verbose();
	if ((err = tape_write_eof(&tape_cap)) != STD_OK) {
		fprintf(stderr, "%s: tape_write_eof error: %s\n", 
			commands[WRITE], err_why(err));
		exit(1);
	}
}
