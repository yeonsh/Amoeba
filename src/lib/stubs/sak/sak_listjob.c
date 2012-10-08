/*	@(#)sak_listjob.c	1.2	94/04/07 11:07:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include	"sak/saksvr.h"

/* sak_list_job ()
 * list a sak job 
 *
 * pjobcap	in	capability of job
 * sched	out	schedule
 * p_opt	out	job options structure
 * return		error status
 */
#ifdef __STDC__
errstat sak_list_job (capability *pjobcap, int8 **sched,
				struct sak_job_options *p_opt)

#else
errstat sak_list_job (pjobcap, sched, p_opt)
capability *pjobcap;
int8 **sched;
struct sak_job_options *p_opt;
#endif /* __STDC__ */
{
	char buf[JFBUFSIZE], *p;
	bufsize result;
	header hdr;
	errstat err = STD_OK;

	hdr.h_port = pjobcap->cap_port;
	hdr.h_priv = pjobcap->cap_priv;
	hdr.h_command = SAK_LIST_JOB;

	result = trans (&hdr, NILBUF, 0, &hdr, buf, JFBUFSIZE);
	if (ERR_STATUS (result))
		return (ERR_CONVERT (result));

	if (hdr.h_status != STD_OK)
		return (ERR_CONVERT(hdr.h_status));

	p = buf_get_sak_job_options (buf, buf + result, p_opt);
	p = buf_get_sched (p, buf + result, sched, &err);

	if (p == (char  *) 0)
		return (STD_ARGBAD);

	return (STD_OK);
}
