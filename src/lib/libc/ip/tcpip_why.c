/*	@(#)tcpip_why.c	1.2	94/04/07 10:34:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
libc/ip/tcpip_why.c

Return a string that decribes an error status returned by a TCP/IP stub.

Created:	Mar 10, 1992 by Philip Homburg
*/

#include <stdlib.h>
#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <server/ip/tcpip.h>

static struct tcpip_err 
{
	errstat error;
	char *message;
} tcpip_errlist[]=
{
	{TCPIP_FAULT,		"bad address"},
	{TCPIP_PACKSIZE,	"invalid packet size"},
	{TCPIP_OUTOFBUFS,	"not enough buffers lef"},
	{TCPIP_BADIOCTL,	"illegal ioctl"},
	{TCPIP_BADMODE,		"bad mode in ioctl"},
	{TCPIP_BADDEST,		"invalid destination address"},
	{TCPIP_DSTNOTRCH,	"destination not reachable"},
	{TCPIP_ISCONN,		"already connected"},
	{TCPIP_ADDRINUSE,	"address in use"},
	{TCPIP_CONNREFUSED,	"connection refused"},
	{TCPIP_CONNRESET, 	"connection reset"},
	{TCPIP_TIMEDOUT, 	"connection timed out"},
	{TCPIP_URG, 		"urgent data present"},
	{TCPIP_NOURG, 		"no urgent data present"},
	{TCPIP_NOTCONN,		"no connection"},
	{TCPIP_SHUTDOWN,	"shutdown connection"},
	{TCPIP_NOCONN,		"no such connection"},
	{TCPIP_ERROR,		"generic error"},
	{0, NULL},		/* End list. */
};

char *tcpip_why(err)
errstat err;
{
	struct tcpip_err *iep;

	for (iep= tcpip_errlist; iep->error != 0; iep++)
		if (iep->error == err)
			return iep->message;
	return err_why(err);
}
