/*	@(#)do_req.c	1.2	94/04/06 11:52:53 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "saksvr.h"
#include "module/rnd.h"

/* save job on disk
 * pjob		in	pointer to job to be saved
 * return		STD_OK succes else error status
 */
static errstat save_job (pjob, ptcap)
struct sak_job *pjob;
capability *ptcap;
{
	char jobfname[FNAME_LEN], *p, *tmpbuf;
	capability filecap;
	extern capability bulletcap;
	errstat err;
 	char transfname[FNAME_LEN];


	if ((err = std_copy (ptcap, ptcap, ptcap)) != STD_OK) {
		fprintf (stderr, "can't copy transfile : %s", err_why (err));
		return (STD_SYSERR);
	}

	/* make sure there are no old files hanging around */
	deletejobfiles (pjob->ident, TRUE);

	(void) sprintf (transfname, "%s/%s%ld", TRANSDIR, TRANSPREFIX,
							(long) pjob->ident);

	if ((err = name_append (transfname, ptcap)) != STD_OK) {
		fprintf (stderr, "cannot append %s to dir : %s\n", transfname,
			 err_why (err));
		std_destroy (ptcap);
		return (STD_SYSERR);
	}

	if ((tmpbuf = (char *) malloc (JFBUFSIZE)) == (char *) 0) {
		memfail (JFBUFSIZE);
		(void) name_delete (transfname);
		(void) std_destroy (ptcap);
		return (STD_NOMEM);
	}

	p = tmpbuf;
	p = buf_put_port	    (p, tmpbuf + JFBUFSIZE, &pjob->rnd);
	p = buf_put_sak_job_options (p, tmpbuf + JFBUFSIZE, &pjob->opt);
	p = buf_put_sched	    (p, tmpbuf + JFBUFSIZE, pjob->schedule);
	if (p == (char *) 0) {
		fprintf (stderr, "JFBUFSIZE to small. Enlarge and recompile\n");
		(void) name_delete (transfname);
		(void) std_destroy (ptcap);
		free ((_VOIDSTAR) tmpbuf);
		return (STD_NOMEM);
	}

	(void) sprintf (jobfname, "%s/%s%ld", JOBDIR, JOBPREFIX,
							(long) pjob->ident);

	if ((err = b_create (&bulletcap, tmpbuf, (b_fsize) (p - tmpbuf),
			      BS_COMMIT, &filecap)) != STD_OK) {
		fprintf (stderr, "cannot create a file for %s : %s\n", jobfname,
			 err_why (err));
		(void) name_delete (transfname);
		(void) std_destroy (ptcap);
		free ((_VOIDSTAR) tmpbuf);
		return (STD_SYSERR);
	}

	free ((_VOIDSTAR) tmpbuf);


	if ((err = name_append (jobfname, &filecap)) != STD_OK) {
		(void) std_destroy (&filecap);
		fprintf (stderr, "cannot append %s to dir : %s\n", jobfname,
			 err_why (err));
		(void) name_delete (transfname);
		(void) std_destroy (ptcap);
		return (STD_SYSERR);
	}

	return (STD_OK);
}

/* uniqjobnr finds uniq jobnr and update maxident
 * function assumes there is uniq jobnr free (max MAX_OBJNUM jobs)
 * return		uniq jobnr
 */
static objnum uniqjobnr ()
{
	extern objnum maxident;

	/* Assume one is free */
	mu_lock (&job_list_sem);
	for (;;) {
		if (maxident == MAX_OBJNUM)
			maxident = (objnum) 1;
		else
			maxident++;

		if (lookup (maxident) == NILCELL)
			break;
	}
	mu_unlock (&job_list_sem);

	return (maxident);
}

/* submit request, get job specs from buffer and create a new job
 * jobnr	in	objnum of capability of request (should be GENERATIC)
 * buf		in	holds job specs
 * bsize	in	size of buf
 * pnewjobnr	out	pointer to jobnr of new created job
 * prnd		out	random field of newly created job 
 * return		STD_OK if succes, else error status
 */
#ifdef __STDC__
errstat sak_submitjob (objnum jobnr, char * buf, bufsize bsize,
						objnum * pnewjobnr, port * prnd)
#else
errstat sak_submitjob (jobnr, buf, bsize, pnewjobnr, prnd)
objnum jobnr;
char *buf;
bufsize bsize;
objnum *pnewjobnr;
port *prnd;
#endif /* __STDC__ */
{
	errstat err;
	struct sak_job *pnewjob;
	capability tcap;
	static mutex exclude_uniq_sem;
	char *p;

	if (jobnr != GENERATIC)
		return (STD_COMBAD);

	*pnewjobnr = NONE;

	if ((pnewjob = newjob ()) == NILCELL)
		return (STD_NOMEM);

	err = STD_OK;
	p = buf;
	p = buf_get_cap		    (p, buf + bsize, &tcap);
	p = buf_get_sak_job_options (p, buf + bsize, &pnewjob->opt);
	p = buf_get_sched	    (p, buf + bsize, pnewjob->schedule, &err);
	if (p != buf + bsize) {
		freejob (pnewjob);
		if (err == STD_NOMEM) {
			MON_EVENT ("out of memory");
			fprintf (stderr,
				"Malloc failed for schedule of newjob\n");
			return (STD_NOMEM);
		} else
			return (STD_ARGBAD);
	}

	if (! valid_sched (pnewjob->schedule)) {
		freejob (pnewjob);
		return (STD_ARGBAD);
	}

#ifdef DEBUG
	pnewjob->ident = 0;
#endif
	if (! schedule (pnewjob, time ((time_t *) 0))) {
		freejob (pnewjob);
		return (SAK_TOOLATE);
	}

	uniqport (&pnewjob->rnd);
	*prnd = pnewjob->rnd;

	mu_lock (&exclude_uniq_sem);	/* uniqjobnr depends on fact that
					* all active jobs are on the joblist
					* so exclude all call to uniqjobnr
					* untill job is in list
					* WARNING : watch out for deadlock
					* I'm going to lock job_list_sem
					* while exclude_uniq_sem is locked
					* as well so don't use exclude_uniq_sem
					* anywhere else
					*/

	*pnewjobnr = pnewjob->ident = uniqjobnr ();

	if ((err = save_job (pnewjob, &tcap)) != STD_OK) {
		freejob (pnewjob);
		return (err);
	}

	mu_lock (&job_list_sem);
		insert_joblist (pnewjob);
		insert_runlist (pnewjob);
	mu_unlock (&job_list_sem);

	mu_unlock (&exclude_uniq_sem);

	return (STD_OK);
}

/* destroy request, if he's got the right cancel job
 * jobnr	in	nr of job to be deleted
 * rights	in	clients rights
 * return		STD_OK if succes, else error status
 */
errstat sak_std_destroy (jobnr, rights)
objnum jobnr;
rights_bits rights;
{
	if (jobnr == GENERATIC)
		return (STD_COMBAD);
	else if (Bitset (rights, OWNERRIGHTS)) {
		remove_job (jobnr, TRUE);
		return (STD_OK);
	}
	else
		return (STD_DENIED);
}

/* info request, return info on jobnr
 * jobnr	in	nr of job
 * rights	in	clients rights
 * buf		out	info on jobnr
 * pbsize	out	size of buf
 * return		STD_OK if succes, else error status
 */
errstat sak_std_info (jobnr, rights, buf, pbsize)
objnum jobnr;
rights_bits rights;
char *buf;
bufsize *pbsize;
{
	int8 deleted;
	struct sak_job *pjob;

	deleted = FALSE;

	if (jobnr == GENERATIC) {
		if (Bitset (rights, OWNERRIGHTS))
			(void) strcpy (buf, "Sak Server (super cap)");
		else
			(void) strcpy (buf, "Sak Server");
	} else
	{
		mu_lock (&job_list_sem);
		pjob = lookup (jobnr);
		if (pjob == NILCELL)
			/* it's just been removed !! */
			deleted = TRUE;
		else
			sprintf (buf, "Sak Job %s", pjob->opt.name);
		mu_unlock (&job_list_sem);
	}

	if (deleted)
	{
		*pbsize = 0;
		return (STD_CAPBAD);
	}

	*pbsize = strlen (buf);

	return (STD_OK);
}

/* age request, if he's got the right start ageing job (done after request is
 * anwsered, by a putreply);
 * jobnr	in	objnum of capability of request (should be GENERATIC)
 * rights	in	clients rights
 * return		STD_OK if succes, else error status
 */
errstat sak_std_age (jobnr, rights)
objnum jobnr;
rights_bits rights;
{
	if (jobnr == GENERATIC) {
		if (Bitset (rights, OWNERRIGHTS)) {
			wait_for_free_thread (NONE, AGE_TYPE);
			return (STD_OK);
		} else
			return (STD_DENIED);
	}
	else
		/* Hmmm, age on a job cap ? */
		return (STD_COMBAD);
}

/* list request, put job spec in buffer
 * jobnr	in	nr of job to be listed
 * rights	in	clients rights
 * buf		out	job specs
 * maxsize	in	maxsize of buf
 * pbsize	out	actual size of buf
 * return		STD_OK if succes, else error status
 */
#ifdef __STDC__
errstat sak_listjob (objnum jobnr, rights_bits rights, char * buf,
					    bufsize maxsize, bufsize * pbsize)
#else
errstat sak_listjob (jobnr, rights, buf, maxsize, pbsize)
objnum jobnr;
rights_bits rights;
char *buf;
bufsize maxsize, *pbsize;
#endif /* __STDC__ */
{
	int8 deleted;
	struct sak_job *pjob;
	char *p;

	*pbsize = 0;

	if (jobnr == GENERATIC)
		return (STD_COMBAD);

	if (! Bitset (rights, OWNERRIGHTS))
		return (STD_DENIED);

	deleted = FALSE;
	mu_lock (&job_list_sem);

	pjob = lookup (jobnr);
	if (pjob != NILCELL)
		/* it's just been removed !! */
		deleted = TRUE;
	else {
		p = buf_put_sak_job_options (buf, buf + maxsize, &pjob->opt);
		p = buf_put_sched (p, buf + maxsize, pjob->schedule);
	}

	mu_unlock (&job_list_sem);

	if (deleted)
		return (STD_CAPBAD);

	if (p == (char *) 0) {
		fprintf (stderr, "Reply buffersize to small for list request");
		return (STD_NOMEM);
	}

	*pbsize = (bufsize) (p - buf);

	return (STD_OK);
}

/* printf error message, called when malloc fails */
/* size		in	# bytes requested in malloc */
void memfail (size)
unsigned size;
{
	MON_EVENT ("out of memory");
	fprintf (stderr, "Couldn't allocate %d bytes\n", size);
}

/* insert a job job into list. Will NOT check for duplicated
 * object numbers !!. 
 * assumes a joblist locked !!
 * pjob	in	pointer to job to be inserted in list
 */
void insert_joblist (pjob)
struct sak_job *pjob;
{
	struct sak_job *ptr, *prev;
	int hval = hashval (pjob->ident);

	ptr = joblist[hval];
	prev = NILCELL;
	while (ptr != NILCELL && ptr->ident < pjob->ident) {
		prev = ptr;
		ptr = ptr->next;
	}

	if (ptr == joblist[hval]) {
		pjob->next = joblist[hval];
		joblist[hval] = pjob;
	}
	else {
		MON_EVENT ("Hash collision");
		pjob->next = ptr;
		prev->next = pjob;
	}
}
/* delete all files associated with job
 * pjob	in	pointer to job
 */
#ifdef __STDC__
void deletejobfiles (objnum jobnr, int8 destroy_transfile)
#else
void deletejobfiles (jobnr, destroy_transfile)
objnum jobnr;
int8 destroy_transfile;
#endif /* __STDC__ */
{
	char buf[FNAME_LEN];
	capability filecap;

	sprintf (buf, "%s/%s%ld", JOBDIR, JOBPREFIX, (long) jobnr);
	if (name_lookup (buf, &filecap) == STD_OK) {
		(void) name_delete (buf);
		(void) std_destroy (&filecap);
	}

	if (! destroy_transfile)
		return;

	sprintf (buf, "%s/%s%ld", TRANSDIR, TRANSPREFIX, (long) jobnr);
	if (name_lookup (buf, &filecap) == STD_OK) {
		(void) name_delete (buf);
		(void) std_destroy (&filecap);
	}
}

/* remove jobnr from list, if found return ptr to job else return NILCELL
 * jobnr	in	job to be removed
 * return		ptr to removed job if found else NILCELL
 * assumes job_list_sem locked !
 */
static struct sak_job *remove_from_joblist (jobnr)
objnum jobnr;
{
	struct sak_job *ptr, *prev;
	int hval = hashval (jobnr);

	ptr = joblist[hval];
	while (ptr != NILCELL && ptr->ident < jobnr) {
		prev = ptr;
		ptr = ptr->next;
	}

	if (ptr == NILCELL || ptr->ident != jobnr)
		return (NILCELL);

	if (ptr == joblist[hval])
		joblist[hval] = ptr->next;
	else
		prev->next = ptr->next;

	return (ptr);
}

/* remove a struct from list and destroy associated files
 * jobnr in	job to be deleted
 */
#ifdef __STDC__
void remove_job (objnum jobnr, int8 destroy_transfile)
#else
void remove_job (jobnr, destroy_transfile)
objnum jobnr;
int8 destroy_transfile;
#endif /* __STDC__ */
{
	struct sak_job *pjob;

	mu_lock (&job_list_sem);
		remove_from_runlist (jobnr);
		pjob = remove_from_joblist (jobnr);
	mu_unlock (&job_list_sem);

	if (pjob != NILCELL) {
		deletejobfiles (jobnr, destroy_transfile);
		freejob (pjob);
	}
}

/* malloc space for new job and initialize it
 * return		pointer to spzce for new job
 */
struct sak_job *newjob ()
{
	struct sak_job *newjob;
	int i;
	
	if ((newjob = (struct sak_job *) malloc (sizeof (struct sak_job))) != 
						(struct sak_job *) 0) {
		newjob->age = MAXAGE;
		newjob->delayed = FALSE;
		newjob->next = newjob->time_next = NILCELL;
		for (i = 0; i < MAX_SPEC; i++)
			newjob->schedule[i] = (int8 *) 0;
	} else {
		MON_EVENT ("out of memory");
		fprintf (stderr, "Couldn't alloc %u bytes\n", 
						sizeof (struct sak_job));
	}

	return (newjob);
}

/* free allocated space of job
 * pjob		in	pointer to job
 */
void freejob (pjob)
struct sak_job *pjob;
{
	int i;

	for (i = 0; i < MAX_SPEC; i++)
		if (pjob->schedule[i] != (int8 *) 0)
			free ((_VOIDSTAR) pjob->schedule[i]);

	free ((_VOIDSTAR) pjob);
}

#ifdef DEBUG
void debug_list_joblist ()
{
	extern mutex tm_sem;
	extern struct sak_job *runlist;
	int i, j = 0;
	struct sak_job *pjob;

	mu_lock (&job_list_sem);
	mu_lock (&tm_sem);

	for (i = 0; i < MODVAL; i++)
		for (pjob = joblist[i]; pjob != NILCELL; pjob = pjob->next) {
			printf (" job %ld(hash %d) named %s (%scatchup,%s result) when : %s",
				(long) pjob->ident, i, pjob->opt.name,
				pjob->opt.catchup ? "" : "no ",
				pjob->opt.save_result ? "save" : "discard",
				ctime (&pjob->executiondate));
			j++;
		}

	if (j == 0)
		printf ("No jobs found\n");

	mu_unlock (&tm_sem);
	mu_unlock (&job_list_sem);
}
#endif
