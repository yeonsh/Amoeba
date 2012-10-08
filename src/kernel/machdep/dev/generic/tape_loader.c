/*	@(#)tape_loader.c	1.5	96/02/27 13:50:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/tape.h"
#include "module/direct.h"

#define TAPE_REC_SZ	(20*512)

errstat
tape_loader(base, size, tapename, tapefile)
char *base;
char *tapename;
long tapefile;
{
	void tape_wait();
	capability tape;
	errstat err;
	char *buf;
	int bytesleft;
	bufsize bytesread;
	int packetpump;
	int32 filepos;

	tape_wait();
	if ((err = dir_lookup((capability *) 0, tapename, &tape)) != STD_OK) {
		printf("Couldn't lookup %s\n", tapename);
		return err;
	}
	if ((err = tape_fpos(&tape, &filepos)) != STD_OK || filepos>=tapefile) {
		if ((err = tape_load(&tape)) != STD_OK) {
			printf("Cannot load tape %s: %s\n", tapename,
							tape_why(err));
			return err;
		}
		filepos=0;
	}
	printf("Searching for tape file %d", tapefile);
	if ((err = tape_fskip(&tape, tapefile-filepos)) != STD_OK) {
		printf("cannot skip %d files on %s: %s\n", tapefile, tapename,
							tape_why(err));
		return err;
	}
	printf(", loading ");
	packetpump=0;
	buf = base;
	bytesleft = size;
	while (bytesleft>0) {
		err = tape_read(&tape, buf,
			bytesleft > TAPE_REC_SZ ?
				TAPE_REC_SZ : (bufsize) bytesleft,
			&bytesread);
		if (err != STD_OK) {
			if (err == TAPE_EOF) {
				printf(" EOF after %dK\n", (size-bytesleft)>>10);
				return STD_OK;
			}
			printf(" Error %d after %dK\n", err, (size-bytesleft)>>10);
			return err;
		}
		printf("%c\b","-\\|/"[(packetpump++)&3]);
		buf += bytesread;
		bytesleft -= bytesread;
	}
	printf("%dK\n", size>>10);
	return STD_OK;
}
