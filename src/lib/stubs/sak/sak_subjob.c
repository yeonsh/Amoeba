/*	@(#)sak_subjob.c	1.2	94/04/07 11:07:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include	"sak/saksvr.h"

/* sak_submit_job ()
 * submit a job to the sakserver
 *
 * svrname	in	pathname of capability of sak server to use
 *			(if 0 default is used)
 * sched	in	schedule
 * ptcap	in	pointer to capability of transaction file
 * p_opt	in	job options structure
 * pjobcap	out	capability of submitted job
 * return		error status
 */
#ifdef __STDC__
errstat sak_submit_job (char *svrname, int8 **sched, capability *ptcap,
			struct sak_job_options *p_opt, capability *pjobcap)
#else
errstat sak_submit_job (svrname, sched, ptcap, p_opt, pjobcap)
char *svrname;
int8 **sched;
capability *ptcap;
struct sak_job_options *p_opt;
capability *pjobcap;
#endif /* __STDC__ */
{
	char buf[JFBUFSIZE], *p;
	capability scap;
	errstat err;
	bufsize result;
	header hdr;

	p = buf_put_cap (buf, buf + JFBUFSIZE, ptcap);
	p = buf_put_sak_job_options (p, buf + JFBUFSIZE, p_opt);
	p = buf_put_sched (p, buf + JFBUFSIZE, sched);

	if (p == (char *) 0) {
		fprintf (stderr,
			 "sak_submit_job : buf[JFBUFSIZE] to small !!\n");
		return (STD_SYSERR);
	}

	err = name_lookup (svrname == (char *) 0 ? DEF_SAKSVR : svrname, &scap);
	if (err != STD_OK)
		return (err);

	hdr.h_port = scap.cap_port;
	hdr.h_priv = scap.cap_priv;
	hdr.h_command = SAK_SUBMIT_JOB;

	result = trans (&hdr, buf, (bufsize) (p - buf), &hdr, NILBUF, 0);
	if (ERR_STATUS (result))
		return (ERR_CONVERT (result));

	if (hdr.h_status != STD_OK)
		return (ERR_CONVERT(hdr.h_status));

	pjobcap->cap_port = hdr.h_port;
	pjobcap->cap_priv = hdr.h_priv;

	return (STD_OK);
}
