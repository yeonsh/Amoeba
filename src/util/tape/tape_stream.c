/*	@(#)tape_stream.c	1.3	96/02/27 13:17:44 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** This module is for Amoeba
**
** tape_stream.c, a program module which holds the tape streaming r/w routines.
** tape_st_read, streams data from tape
** tape_st_write, streams data to tape
**
** peterb@cwi.nl, 120190
**
*/

#include <stdio.h>
#include <amtools.h>
#include <module/tape.h>
#include "commands.h"
#include "stdlib.h"
#include "thread.h"

/*
** global variables.
**
*/
extern errstat		err;
extern bufsize		actual;
extern long		in;
extern long		out;
extern int		record_size;
extern long		count;
extern int		verbose;
extern long		buffer_count;
extern capability	tape_cap;
extern char *		cmd;
extern char *		commands[];

static bufptr      stream_buf[MAX_NUM_BUFS];
static int	   stream_buf_size[MAX_NUM_BUFS];
static mutex       stream_buf_full[MAX_NUM_BUFS];
static mutex	   stream_buf_empty[MAX_NUM_BUFS];


/*
** This thread has a small ugliness:
**	It returns only when the count gets to 0
*/
static void
stdin_read_thread(p, s)
char *	p;
int	s;
{
	int    num_bytes, tmp_size;
	int    cur_buf = 0;
	bufptr tmp_buf;

	while (count == -1 || count > 0) {

		/*
		** Allocate empty buffer...
		**
		*/
		mu_lock(&stream_buf_empty[cur_buf]);

		tmp_size = record_size;
		tmp_buf  = stream_buf[cur_buf];
		do {
			num_bytes = read(fileno(stdin), tmp_buf, tmp_size);
			if (num_bytes == -1) {
				if (verbose)
					write_verbose();
				perror("read - stdin");
				exit(1);
			}

			if (num_bytes == 0)		/* Got an EOF */
				break;

			tmp_buf  += num_bytes;
			tmp_size -= num_bytes;
		} while (tmp_size > 0);

		if (record_size - tmp_size > 0) {
			in++;
			while (tmp_size > 0) {
			    *tmp_buf++ = '\0';
			    tmp_size--;
			}
		}

		stream_buf_size[cur_buf] = record_size - tmp_size;
		mu_unlock(&stream_buf_full[cur_buf]);

		cur_buf = (cur_buf + 1) % buffer_count;
		if (count > 0)
			count--;

		/* We must stop if we found eof! */
		if (tmp_size != 0)
		    break;
	}

	/*
	** We're ready, inform main thread by means of an empty buffer...
	**
	*/
	mu_lock(&stream_buf_empty[cur_buf]);
	stream_buf_size[cur_buf] = 0;
	mu_unlock(&stream_buf_full[cur_buf]);
}


static void
tape_read_thread(p, s)
char *	p;
int	s;
{
	int     cur_buf = 0;

	err = STD_OK;
	while (count == -1 || count > 0) {

		/*
		** Allocate empty buffer...
		**
		*/
		mu_lock(&stream_buf_empty[cur_buf]);

		err = tape_read(&tape_cap, (bufptr)stream_buf[cur_buf], 
				(bufsize) record_size, &actual);

		if (err == TAPE_EOF)		/* Got an EOF */
			break;

		if (actual != 0)
		    in++;
		if (err != STD_OK) {
			if (verbose)
				write_verbose();
			fprintf(stderr, 
				"%s: tape_read error: %s (actual read 0x%x)\n",
				commands[READ], tape_why(err), actual);
			exit(1);
		}
	
		stream_buf_size[cur_buf] = actual;
		mu_unlock(&stream_buf_full[cur_buf]);

		cur_buf = (cur_buf + 1) % buffer_count;
		if (count > 0)
			count--;
	}

	/*
	** We're ready, inform main thread by means of an empty buffer...
	** If we exited the above loop due to EOF then we don't lock the
	** buffer since we still had it locked.  Otherwise we do.
	*/
	if (err != TAPE_EOF)
		mu_lock(&stream_buf_empty[cur_buf]);
	stream_buf_size[cur_buf] = 0;
	mu_unlock(&stream_buf_full[cur_buf]);
}


static int
allocate_bufs(n)
	int n;
{
	int i;

	if (buffer_count > MAX_NUM_BUFS) {
		fprintf(stderr, "%s: buffer_count may not exceed %d\n",
			commands[n], MAX_NUM_BUFS);
		exit(1);
	}

	if (buffer_count < 1) {
		fprintf(stderr, "%s: buffer_count may not be less than 1\n",
			commands[n]);
		exit(1);
	}

	for (i = 0; i != buffer_count; i++)
		if ((stream_buf[i] = (bufptr)malloc((size_t) record_size))
									== 0) {
			fprintf(stderr, 
				"%s: cannot allocate transaction buffer.\n", 
				commands[n]);
			exit(1);
		}
}

/*
** mtape_read, stream data from tape...
**
*/
void
mtape_read()
{
	int cur_buf = 0, n;

	allocate_bufs(READ);

	/*
	** Initialize mutexes and lock buffers (not filled)...
	**
	*/
	for (n = 0; n != buffer_count; n++) {
		mu_init(&stream_buf_empty[n]);
		mu_init(&stream_buf_full[n]);
		mu_lock(&stream_buf_full[n]);
	}

	/*
	** Spawn a reader thread...
	**
	*/
	if (!thread_newthread(tape_read_thread, 0x1000, (char *)0, 0)) {
		fprintf(stderr, "%s: spawning of reader thread failed.\n",
			commands[READ]);
		exit(1);
	}

	/*
	** See if we still have something to do...
	**
	*/
	while (1) {

		/*
		** Allocate current buffer...
		**
		*/
		mu_lock(&stream_buf_full[cur_buf]);

		if (stream_buf_size[cur_buf] > 0) {
			if (write(fileno(stdout), stream_buf[cur_buf], 
				stream_buf_size[cur_buf]) != 
				stream_buf_size[cur_buf]) {
				if (verbose)
					write_verbose();
				perror("write - stdout");
				exit(1);
			}
			out++;
		}
		if (stream_buf_size[cur_buf] != record_size) {
			if (verbose)
				write_verbose();
			return;
		}

		/*
		** Release the buffer...
		**
		*/
		mu_unlock(&stream_buf_empty[cur_buf]);
		cur_buf = (cur_buf + 1) % buffer_count;
	}
}


/*
** mtape_write, stream data to tape.
**
*/
void
mtape_write()
{
	int cur_buf = 0;
	int n;

	allocate_bufs(WRITE);

	/*
	** Initialize mutexes and lock buffers (not filled)...
	**
	*/
	for (n = 0; n != buffer_count; n++) {
		mu_init(&stream_buf_empty[n]);
		mu_init(&stream_buf_full[n]);
		mu_lock(&stream_buf_full[n]);
	}

	/*
	** Spawn a reader thread...
	**
	*/
	if (!thread_newthread(stdin_read_thread, 0x1000, (char *)0, 0)) {
		fprintf(stderr, "%s: spawning of reader thread failed.\n",
			commands[WRITE]);
		exit(1);
	}

	/*
	** See if we still have something to do...
	**
	*/
	while (1) {

		/*
		** Allocate current buffer...
		**
		*/
		mu_lock(&stream_buf_full[cur_buf]);

		if (stream_buf_size[cur_buf] > 0) {
			err = tape_write(&tape_cap, stream_buf[cur_buf], 
				(bufsize) stream_buf_size[cur_buf], &actual);
			if (actual != 0)
			    out++;
			if (err != STD_OK) {
				if (verbose)
					write_verbose();
				fprintf(stderr, 
					"%s: tape_write error: %s (actual written 0x%x)\n", 
					commands[WRITE], tape_why(err), 
					actual);
				exit(1);
			}
		}

		if (stream_buf_size[cur_buf] != record_size)
			break;

		/*
		** Release the buffer...
		**
		*/
		mu_unlock(&stream_buf_empty[cur_buf]);
		cur_buf = (cur_buf + 1) % buffer_count;
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
