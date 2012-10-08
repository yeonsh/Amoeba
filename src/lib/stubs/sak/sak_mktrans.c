/*	@(#)sak_mktrans.c	1.2	94/04/07 11:07:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include	"sak/saksvr.h"

/* sak_make_transfile ()
 *
 * store transaction in a file and return capability to this file
 * phdr		in	pointer to transaction header
 * tbuf		in	transaction buffer
 * req_size	in	size of transaction buffer
 * reply_size	in	expected return buffer size of transaction
 * wait		in	transaction timeout
 * pcap		out	pointer to capability of transaction file
 * return		error status
 */
#ifdef __STDC__
errstat sak_make_transfile(header *phdr, char *tbuf, bufsize req_size,
			   bufsize reply_size, interval wait, capability *pcap)
#else
errstat sak_make_transfile (phdr, tbuf, req_size, reply_size, wait, pcap)
header *phdr;
char *tbuf;
bufsize req_size, reply_size;
interval wait;
capability *pcap;
#endif /* __STDC__ */
{
	char buf[TRANS_INFO_BUF_SIZE], *p;
	struct trans_info tr_info;
	capability bulletcap;
	errstat err;
	int cflag;

	tr_info.hdr	   = *phdr;
	tr_info.req_size   = req_size;
	tr_info.reply_size = reply_size;
	tr_info.timeout    = wait;

	if ((err = name_lookup  (DEF_BULLETSVR, &bulletcap)) != STD_OK)
		return (err);

	p = buf_put_trans_info (buf, buf + TRANS_INFO_BUF_SIZE, &tr_info);

	if (p == (char *) 0) {
		fprintf (stderr,
		      "sak_make_transfile : TRANS_INFO_BUF_SIZE to small !!\n");
		return (STD_SYSERR);
	}

	cflag = req_size > 0 ? 0 : BS_COMMIT;

	err = b_create (&bulletcap, buf, (b_fsize) (p - buf), cflag, pcap);

	if (err != STD_OK || req_size <= 0)
		return (err);
	
	err = b_modify (pcap, (b_fsize) (p - buf), tbuf, (b_fsize) req_size,
				BS_COMMIT, pcap);
	return (err);
}
