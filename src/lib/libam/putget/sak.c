/*	@(#)sak.c	1.2	94/04/07 10:18:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* buf.c
 *
 * routines used to put/get sak data structures in/out buffers
 * syntax is the same as all buf_get/put routines
 * except for buf_get_schedule, which takes a fourth argument :
 * a pointer to a errstat. This is since buf_get_schedule will 
 * malloc space for the schedule lines. On exit 3 values
 * for perr are possible : STD_OK, STD_NOMEM, STD_BADARG.
 * Space will only be malloced when perr == STD_OK.
 */

#include	"sak/saksvr.h"
#include	"module/buffers.h"
#include	"caplist.h"

#ifdef __STDC__
char * buf_get_sak_job_options (char * buf, char * endbuf,
						struct sak_job_options *p_opt)
#else
char * buf_get_sak_job_options (buf, endbuf, p_opt)
char *buf, *endbuf;
struct sak_job_options *p_opt;
#endif /* __STDC__ */
{
	char *p, *ptr;

	p = buf;
	if (p == (char *) 0) return ((char *) 0);

	p = buf_get_cap		(p, endbuf, &p_opt->where);
	p = buf_get_int8	(p, endbuf, &p_opt->catchup);
	p = buf_get_int8	(p, endbuf, &p_opt->save_result);
	p = buf_get_string	(p, endbuf, &ptr);

	if (p != (char *) 0) {
		(void) strncpy (p_opt->name, ptr, MAX_NAME_LEN);
		p_opt->name[MAX_NAME_LEN - 1] = '\0';
	}

	return (p);
}

#ifdef __STDC__
char * buf_put_sak_job_options ( char * buf, char * endbuf,
						struct sak_job_options *p_opt)
#else
char * buf_put_sak_job_options (buf, endbuf, p_opt)
char *buf, *endbuf;
struct sak_job_options *p_opt;
#endif /* __STDC__ */
{
	char *p;

	p = buf;
	if (p == (char *) 0) return ((char *) 0);
	p = buf_put_cap		(p, endbuf, &p_opt->where);
	p = buf_put_int8	(p, endbuf, p_opt->catchup);
	p = buf_put_int8	(p, endbuf, p_opt->save_result);
	p = buf_put_string	(p, endbuf, p_opt->name);
	return (p);
}

#ifdef __STDC__
char * buf_put_sched (char * buf, char * endbuf, int8 ** sched)
#else
char * buf_put_sched (buf, endbuf, sched)
char *buf, *endbuf;
int8 **sched;
#endif /* __STDC__ */
{
	char *p;
	int i, j;

	p = buf;
	if (p == (char *) 0) return ((char *) 0);

	for (i = 0; i < MAX_SPEC; i++) {
		j = 0;
		do
			p = buf_put_int8 (p, endbuf, sched[i][j]);
		while (sched[i][j++] != DELIMITER);
	}

	return (p);
}

#ifdef __STDC__
static char * get_schedline (char * buf, char * endbuf, int8 ** schedline,
								errstat * perr)
#else
static char * get_schedline (buf, endbuf, schedline, perr)
char *buf, *endbuf;
int8 **schedline;
errstat *perr;
#endif /* __STDC__ */
{
	char *p;
	int i, size;
	int8 val;

	p = buf;
	if (p == (char *) 0) return ((char *) 0);

	size = 0;
	do {		/* determine length, so we can malloc space */
		p = buf_get_int8 (p, endbuf, &val);
		if (p == (char *) 0 || ++size > MAX_SPEC_SIZE) {
			*perr = STD_ARGBAD;
			return ((char *) 0);
		}
	} while (val != DELIMITER);

	if ((*schedline = (int8 *) malloc (size * sizeof (int8))) ==
							(int8 *) 0) {
		*perr = STD_NOMEM;
		return ((char *) 0);
	}

	p = buf; /* reset pointer */

	for (i = 0; i < size; i++)
		/* cannot fail */
		p = buf_get_int8 (p, endbuf, &(*schedline)[i]);

	*perr = (p == (char *) 0) ? STD_ARGBAD : STD_OK;
	return (p);
}

#ifdef __STDC__
char * buf_get_sched (char * buf, char * endbuf, int8 ** sched, errstat *perr)
#else
char * buf_get_sched (buf, endbuf, sched, perr)
char *buf, *endbuf;
int8 **sched;
errstat *perr;
#endif /* __STDC__ */
{
	char *p;
	int i;

	p = buf;
	if (p == (char *) 0) return ((char *) 0);

	for (i = 0; i < MAX_SPEC; i++) {
		sched[i] = (int8 *) 0;
		p = get_schedline (p, endbuf, &sched[i], perr);
		if (p == (char *) 0)
			break;
	}

	if (i < MAX_SPEC) { /* something failed, free all malloc lines */
		do {
			if (sched[i] != (int8 *) 0) {
				free (sched[i]);
				sched[i] = (int8 *) 0;
			}
		} while (--i >= 0);
		return ((char *) 0);
	}
	else
		return (p);
}

#ifdef __STDC__
char * buf_put_trans (char * buf, char * endbuf, header * phdr)
#else
char * buf_put_trans (buf, endbuf, phdr)
char *buf, *endbuf;
header *phdr;
#endif /* __STDC__ */
{
	char *p;

	p = buf;
	if (p == (char *) 0) return ((char *) 0);

	p = buf_put_port    (p, endbuf, &phdr->h_port);
	p = buf_put_port    (p, endbuf, &phdr->h_signature);
	p = buf_put_priv    (p, endbuf, &phdr->h_priv);
	p = buf_put_command (p, endbuf, phdr->h_command);
	p = buf_put_int32   (p, endbuf, phdr->h_offset);
	p = buf_put_bufsize (p, endbuf, phdr->h_size);
	p = buf_put_int16   (p, endbuf, phdr->h_extra);
	return (p);
}

#ifdef __STDC__
char * buf_get_trans (char * buf, char * endbuf, header * phdr)
#else
char * buf_get_trans (buf, endbuf, phdr)
char *buf, *endbuf;
header *phdr;
#endif /* __STDC__ */
{
	char *p;

	p = buf;
	if (p == (char *) 0) return ((char *) 0);

	p = buf_get_port    (p, endbuf, &phdr->h_port);
	p = buf_get_port    (p, endbuf, &phdr->h_signature);
	p = buf_get_priv    (p, endbuf, &phdr->h_priv);
	p = buf_get_command (p, endbuf, &phdr->h_command);
	p = buf_get_int32   (p, endbuf, &phdr->h_offset);
	p = buf_get_bufsize (p, endbuf, &phdr->h_size);
	p = buf_get_uint16   (p, endbuf, &phdr->h_extra);

	return (p);
}

#ifdef __STDC__
char * buf_get_trans_info (char * buf, char * endbuf,
						struct trans_info * ptinfo)
#else
char * buf_get_trans_info (buf, endbuf, ptinfo)
char *buf, *endbuf;
struct trans_info *ptinfo;
#endif /* __STDC__ */
{
	char *p;

	p = buf;
	if (p == (char *) 0) return ((char *) 0);

	p = buf_get_trans    (p, endbuf, &ptinfo->hdr);
	p = buf_get_bufsize  (p, endbuf, &ptinfo->req_size);
	p = buf_get_bufsize  (p, endbuf, &ptinfo->reply_size);
	p = buf_get_interval (p, endbuf, &ptinfo->timeout);

	return (p);
}

#ifdef __STDC__
char * buf_put_trans_info (char * buf, char * endbuf,
						struct trans_info * ptinfo)
#else
char * buf_put_trans_info (buf, endbuf, ptinfo)
char *buf, *endbuf;
struct trans_info *ptinfo;
#endif /* __STDC__ */
{
	char *p;

	p = buf;
	if (p == (char *) 0) return ((char *) 0);

	p = buf_put_trans    (p, endbuf, &ptinfo->hdr);
	p = buf_put_bufsize  (p, endbuf, ptinfo->req_size);
	p = buf_put_bufsize  (p, endbuf, ptinfo->reply_size);
	p = buf_put_interval (p, endbuf, ptinfo->timeout);

	return (p);
}

#ifdef __STDC__
char * buf_put_char (char * buf, char * endbuf, char ch)
#else
char * buf_put_char (buf, endbuf, ch)
char *buf, *endbuf, ch;
#endif /* __STDC__ */
{
	if (buf == (char *) 0 || buf + 1 > endbuf) return ((char *) 0);

	*buf++ = ch;

	return (buf);
}

#ifdef __STDC__
char * buf_get_char (char * buf, char * endbuf, char * pch)
#else
char * buf_get_char (buf, endbuf, pch)
char *buf, *endbuf, *pch;
#endif /* __STDC__ */
{
	if (buf == (char *) 0 || buf + 1 > endbuf) return ((char *) 0);

	*pch = *buf++;

	return (buf);
}


/* WARNING strings are NOT dupped so the point to space in the specified
 * buffer
 */
#ifdef __STDC__
char * buf_get_argvptr (char * buf, char * endbuf, char *** pargv,
							int max, errstat * perr)
#else
char * buf_get_argvptr (buf, endbuf, pargv, max, perr)
char *buf, *endbuf;
char ***pargv;
int max;
errstat *perr;
#endif /* __STDC__ */
{
	char *p;
	int16 nr, i;

	p = buf;
	if (p == (char *) 0) return ((char *) 0);
	p = buf_get_int16 (p, endbuf, &nr);
	if (nr < 0 || nr > max)
	{
		*perr = SAK_ARGTOOBIG;
		return ((char *) 0);
	}
	if (p == (char *) 0) return ((char *) 0);
	if ((*pargv = (char **)
		malloc ((nr + 1) * sizeof (char *))) == (char **) 0)
	{
		*perr = STD_NOMEM;
		return ((char *) 0);
	}
	for (i = 0; i < nr; i++)
	{
		p = buf_get_string (p, endbuf, *pargv + i);
		if (p == (char *) 0)
			return ((char *) 0);
	}
	
	(*pargv) [i] = (char *) 0;

	return (p);
}

#ifdef __STDC__
void buf_free_argvptr (char ** argv)
#else
void buf_free_argvptr (argv)
char **argv;
#endif /* __STDC__ */
{
	free (argv);
}

#ifdef __STDC__
char * buf_put_argvptr (char * buf, char * endbuf, char ** argv)
#else
char * buf_put_argvptr (buf, endbuf, argv)
char *buf, *endbuf;
char **argv;
#endif /* __STDC__ */
{
	int16 nr = 0, i;
	char *p;

	while (argv[nr] != (char *) 0)
		nr++;
	
	p = buf;
	p = buf_put_int16 (p, endbuf, nr);
	for (i = 0; i < nr; i++)
		p = buf_put_string (p, endbuf, argv[i]);
	
	return (p);
}

/* WARNING strings are NOT dupped so the point to space in the specified
 * buffer. Space for capabilities IS malloced and has to be freed later.
 */
#ifdef __STDC__
char * buf_get_caplptr (char * buf, char * endbuf, struct caplist ** pcapl,
							int max, errstat * perr)
#else
char * buf_get_caplptr (buf, endbuf, pcapl, max, perr)
char *buf, *endbuf;
struct caplist **pcapl;
int max;
errstat *perr;
#endif /* __STDC__ */
{
	char *p;
	int16 nr, i;

	p = buf;
	if (p == (char *) 0) return ((char *) 0);
	p = buf_get_int16 (p, endbuf, &nr);
	if (p == (char *) 0) return ((char *) 0);
	if (nr < 0 || nr > max)
	{
		*perr = SAK_ENVTOOBIG;
		return ((char *) 0);
	}
	if ((*pcapl = (struct caplist *)
		malloc ((nr + 1) *
			sizeof (struct caplist))) == (struct caplist *) 0)
	{
		*perr = STD_NOMEM;
		return ((char *) 0);
	}
	for (i = 0; i < nr; i++)
	{
		p = buf_get_string (p, endbuf, & (*pcapl) [i].cl_name);
		if (((*pcapl) [i].cl_cap = (capability *)
			malloc (sizeof (capability))) == (capability *) 0)
		{
			*perr = STD_NOMEM;
			return ((char *) 0);
		}
		p = buf_get_cap (p, endbuf, (*pcapl) [i].cl_cap);
		if (p == (char *) 0)
			return ((char *) 0);
	}
	(*pcapl) [i].cl_name = (char *) 0;
	(*pcapl) [i].cl_cap = (capability *) 0;


	return (p);
}

#ifdef __STDC__
void buf_free_caplptr (struct caplist * capl)
#else
void buf_free_caplptr (capl)
struct caplist *capl;
#endif /* __STDC__ */
{
	int i;

	i = 0;
	while (capl[i].cl_name != (char *) 0)
		free (capl[i++].cl_cap);
	
	free (capl);
}

#ifdef __STDC__
char * buf_put_caplptr (char * buf, char * endbuf, struct caplist * capl)
#else
char * buf_put_caplptr (buf, endbuf, capl)
char *buf, *endbuf;
struct caplist *capl;
#endif /* __STDC__ */
{
	int16 nr = 0, i;
	char *p;

	p = buf;
	while (capl[nr].cl_name != (char *) 0)
		nr++;

	p = buf_put_int16 (p, endbuf, nr);
	for (i = 0; i < nr; i++)
	{
		p = buf_put_string (p, endbuf, capl[i].cl_name);
		p = buf_put_cap    (p, endbuf, capl[i].cl_cap);
	}

	return (p);
}
