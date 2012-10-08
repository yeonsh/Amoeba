/*	@(#)isamdir.c	1.2	94/04/07 10:08:10 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Return true if a capability refers to an Amoeba directory */

#include "amoeba.h"
#include "stderr.h"
#include "module/stdcmd.h"

#define DIR_INFO(str)	(str[0] == '/' || str[0] == '%')

int
_is_am_dir(pcap)
	capability *pcap;
{
	char     infobuf[30];
	interval savetout;
	int      len;
	errstat  err;
	
	/* We assume the capability refers to a directory when
	 * the server returns a std_info string that is typical for
	 * directory servers, i.e., starting with '/' or '%' (kernel dir).
	 */
	savetout = timeout((interval) 2000);
	err = std_info(pcap, infobuf, (int) sizeof(infobuf), &len);
	timeout(savetout);

	return (err == STD_OK || err == STD_OVERFLOW) &&
	       (len > 0) && DIR_INFO(infobuf);
}
