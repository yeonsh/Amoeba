/*	@(#)handle_work.c	1.2	94/04/06 11:53:11 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "saksvr.h"

static errstat find_info _ARGS((capability *, struct sak_job_options *,
							struct trans_info *));
static void send_ok _ARGS((struct sak_job_options * p_opt, bufsize reply_size));
static errstat do_trans _ARGS((struct sak_job_options *, struct trans_info *));
static void append_to_statusfile  _ARGS((char *, capability *));
static void append_to_locked_statusfile  _ARGS((char *, capability *));

/* handle_work ()
 * execute job specified by jobnr
 * jobnr	in	job number
 *
 * A job consists of one transaction (for now). The transaction is
 * taken from a user specified file (as made by sak_make_transfile (L)). 
 * actions:
 *	1) get job structure
 *	2) reschedule job, if done remove job from list
 *	3) get the transaction out of the file
 *	4) do transaction
 *	5) append result to user specified status file
 *
 * For now a job can only be one transaction. It would not be difficult
 * to adjust it to handle more. Problem is wat is usefull ?
 * Specifying 2 alternative transactions, one when last transaction
 * succeded (Define succeded ?) one when failed. How about results ?
 * Most likely you want to use certain parameters/results from the last
 * transaction in the next (That won't be easy).
 */
void handle_work (jobnr)
objnum jobnr;
{
	struct sak_job_options opt;
	struct trans_info job_trans;
	struct sak_job *pjob;
	errstat err;
	int8 delete_me;
	char tfilename[FNAME_LEN];
	capability tcap;

	delete_me = FALSE;
	mu_lock (&job_list_sem);

	pjob = lookup (jobnr);
	if (pjob != NILCELL) {
		opt = pjob->opt;
		opt.catchup = pjob->delayed; /* save delayed status in
					      * catchup is not appropriate
					      * but it's easy
					      */
		pjob->delayed = FALSE;
		pjob->opt.catchup = FALSE;
#ifdef DEBUG
		if (debug > 1) printf ("Starting work on jobnr %ld named %s\n", (long) jobnr, opt.name);
#endif
		if (! schedule (pjob, pjob->executiondate))
			delete_me = TRUE;
		else
			insert_runlist (pjob);
	}

	mu_unlock (&job_list_sem);

	if (pjob == NILCELL) { /* must have been deleted */
#ifdef DEBUG
		if (debug > 1) printf ("No work, jobnr %ld has been deleted\n", (long) jobnr);
#endif
		return;
	}

	if (delete_me)
		remove_job (jobnr, FALSE);


	
	sprintf (tfilename, "%s/%s%ld", TRANSDIR, TRANSPREFIX, (long) jobnr);
	if ((err = name_lookup (tfilename, &tcap)) != STD_OK) {
		fprintf (stderr, "Can't get cap for %s : %s\n", tfilename,
			 err_why (err));
		send_failure (&opt,
			      "(internal) Can't get transaction file", err);
	} else {
		if ((err = find_info (&tcap, &opt, &job_trans)) == STD_OK)
			err = do_trans (&opt, &job_trans);

		if (job_trans.buf != (char *) 0)
			free ((_VOIDSTAR) job_trans.buf);
	}
	
	if (delete_me) {
		(void) name_delete (tfilename);
		(void) std_destroy (&tcap);
	}

#ifdef DEBUG
	if (debug > 1) printf ("Finished work on %sjobnr %ld (job %s)\n",
				opt.catchup ? "delayed " : "", (long) jobnr,
				err == STD_OK ? "succesfull" : "failed");
#endif
}

/* find_info ()
 *
 * find_info reads a transaction out of a transaction file as made by
 * sak_make_transfile (L) it allocates a buffer then reads the file
 * into it. Fills the fields in the trans_info struct ptrans. 
 * ptcap	in	pointer to transaction file capability
 * p_opt	in	pointer to job options (used to send message)
 * ptrans	in/out	pointer to transaction info. fields will be filled
 * return		error status
 *
 */
static errstat find_info (ptcap, p_opt, ptrans)
capability *ptcap;
struct sak_job_options *p_opt;
struct trans_info *ptrans;
{
	b_fsize offset, lsize, blsize;
	errstat err;
	char *tmpbuf, *p;

	if ((tmpbuf = (char *) malloc (TRANS_INFO_BUF_SIZE)) == (char *) 0) {
		memfail (TRANS_INFO_BUF_SIZE);
		send_failure (p_opt,
			      "(internal) couldn't malloc buffer for trans",
			      (errstat) STD_NOMEM);
		return (STD_NOMEM);
	}

	if ((err = b_read (ptcap, (b_fsize) 0, tmpbuf, (b_fsize)
			TRANS_INFO_BUF_SIZE, &blsize)) != STD_OK) {
		fprintf (stderr, "cannot read transaction file : %s\n",
								err_why (err));
		free ((_VOIDSTAR) tmpbuf);
		send_failure (p_opt,
			      "(internal) couldn't transaction file", err);
		return (err);
	}

	p = buf_get_trans_info (tmpbuf, tmpbuf + blsize, ptrans);

	offset = (p - tmpbuf);
	
	free ((_VOIDSTAR) tmpbuf);

	if (p == (char *) 0) {
		fprintf (stderr, "Bad trans spec in transfile : size ");
		fprintf (stderr, "in buffer bigger then size in struct ?\n");
		send_failure (p_opt,
			      "bad transaction specified in transaction file",
			      (errstat) STD_ARGBAD);
		return (STD_ARGBAD);
	}

#if 0
	if (ptrans->req_size < 0) ptrans->req_size = 0;
	if (ptrans->reply_size < 0) ptrans->reply_size = 0;
#endif
	ptrans->buf = (char *) 0;

	lsize = ptrans->req_size > ptrans->reply_size ?
			ptrans->req_size : ptrans->reply_size;

	ptrans->buf = (char *) 0;

	if (lsize == 0) /* no buffer needed at all */
		return (STD_OK);

	if ((ptrans->buf = (char *) malloc ((unsigned) lsize)) == (char *) 0) {
		memfail ((unsigned) lsize);
		send_failure (p_opt,
			      "(internal) couldn't malloc buffer for trans",
			      (errstat) STD_NOMEM);
		return (STD_NOMEM);
	}

	if (ptrans->req_size == 0) /* no req buffer so no need to read any
				    * futhur */
		return (STD_OK);

	if ((err = b_read (ptcap, offset, ptrans->buf,
			(b_fsize) ptrans->req_size, &blsize)) != STD_OK) {
		fprintf (stderr, "cannot read transaction file : %s",
							err_why (err));
		free ((_VOIDSTAR) ptrans->buf);
		send_failure (p_opt,
			      "(internal) couldn't read transaction file", err);
		return (err);
	}

	if (blsize < ptrans->req_size) ptrans->req_size = blsize;
	
	return (STD_OK);
}

/* do_trans ()
 * 
 * Execute the transaction specified in ptrans
 *
 * p_opt	in	pointer to job options
 * ptrans	in	pointer to struct with transaction info
 * return		error status
 */
static errstat do_trans (p_opt, ptrans)
struct sak_job_options *p_opt;
struct trans_info *ptrans;
{
	interval save_timeout;
	bufsize reply_size;

	save_timeout = timeout (ptrans->timeout > MAX_TIMEOUT ?
					MAX_TIMEOUT : ptrans->timeout);

	reply_size = trans (&ptrans->hdr, ptrans->buf, ptrans->req_size,
			    &ptrans->hdr, ptrans->buf, ptrans->reply_size);
	
	(void) timeout (save_timeout);

	if (ERR_STATUS (reply_size)) {
		send_failure (p_opt, "transaction failed ",
					(errstat) ERR_CONVERT (reply_size));
		return ((errstat) ERR_CONVERT (reply_size));
	} else {
		send_ok (p_opt, reply_size /* , ptrans->buf*/);
		return (STD_OK);
	}
}

/* append_to_statusfile ()
 * 
 * append buffer to user status file. Lock all status updates to
 * avoid raise conditions. (Should be able to lock specific status file)
 *
 * buf		in	buffer to be appended
 * pwhere	in	pointer to capabilty of user status file
 */
static void append_to_statusfile (buf, pwhere)
char *buf;
capability *pwhere;
{
	static mutex status_file_update;

	mu_lock (&status_file_update);
	append_to_locked_statusfile (buf, pwhere);
	mu_unlock (&status_file_update);
}

/* append_to_statusfile ()
 * 
 * append buffer to user status file. Assumes a locked status file
 *
 * buf		in	buffer to be appended
 * pwhere	in	pointer to capabilty of user status file
 */
static void append_to_locked_statusfile (buf, pwhere)
char *buf;
capability *pwhere;
{
	capability filecap, newfilecap;
	extern capability bulletcap;
	errstat err;
	b_fsize lsize;

	if ((err = dir_lookup (pwhere, "sak_status", &filecap)) != STD_OK) {
		if (err != STD_NOTFOUND) {
			fprintf (stderr, "cannot find a status file : %s\n", 
							 err_why (err));
			return;
		}
		if ((err = b_create (&bulletcap, buf, (b_fsize) strlen (buf),
					BS_COMMIT, &filecap)) != STD_OK) {
			fprintf (stderr, "cannot create a status file : %s\n", 
						 err_why (err));
			return;
		}
		if ((err = dir_append (pwhere, "sak_status", &filecap)) !=
									STD_OK)
			fprintf (stderr, "cannot append a status file : %s\n", 
							 err_why (err));
		return;
	} else {
		if ((err = b_size (&filecap, &lsize)) != STD_OK) {
			fprintf (stderr,
				"cannot get size of a status file : %s\n", 
				 err_why (err));
			return;
		}
		if ((err = b_modify (&filecap, lsize, buf, (b_fsize)
			     strlen (buf), BS_COMMIT, &newfilecap)) != STD_OK) {
			fprintf (stderr, "cannot create a status file : %s\n", 
						 err_why (err));
			return;
		}
		if ((err = dir_replace (pwhere, "sak_status", &newfilecap)) !=
								STD_OK) {
			fprintf (stderr, "cannot replace a status file : %s\n", 
							 err_why (err));
			(void) std_destroy (&newfilecap);
			return;
		}
		(void) std_destroy (&filecap);
		return;
	}
}

/* send_failure ()
 * 
 * append a failure message to user status file
 *
 * p_opt	in	pointer to job options
 * reason	in	reason why job failed
 * r_err	in	error that occured
 */
void send_failure (p_opt, reason, r_err)
struct sak_job_options *p_opt;
char *reason;
errstat r_err;
{
	time_t now;
	char buf[256];
	extern mutex tm_sem;

	if (! p_opt->save_result)
		return;

	(void) time (&now);
	mu_lock (&tm_sem);
	sprintf (buf, "Your %ssak transaction %s failed at %s",
		 p_opt->catchup ? "delayed " : "", p_opt->name, ctime (&now));
	mu_unlock (&tm_sem);
	sprintf (buf + strlen (buf),
			"Reason : %s (error code %d)\n", reason, r_err);

	append_to_statusfile (buf, &p_opt->where);
}

/* send_ok ()
 * 
 * append a succes message to user status file.
 * For now reply_buffer are ignored, could be saved
 *
 * p_opt	in	pointer to job options
 * reply_size	in	size of reply from transaction
 * reply_buf	in	reply from transaction (not used)
 */
#ifdef __STDC__
static void send_ok (struct sak_job_options * p_opt, bufsize reply_size)
#else
static void send_ok (p_opt, reply_size /* , reply_buf*/)
struct sak_job_options *p_opt;
bufsize reply_size;
/*
char *reply_buf;
*/
#endif /* __STDC__ */
{
	time_t now;
	char buf[256];
	extern mutex tm_sem;

	if (! p_opt->save_result)
		return;

	(void) time (&now);
	mu_lock (&tm_sem);
	sprintf (buf, "Your %ssak transaction %s succeded at %s", 
		 p_opt->catchup ? "delayed " : "", p_opt->name, ctime (&now));
	mu_unlock (&tm_sem);

	sprintf (buf + strlen (buf), "return size was %ld\n",
							(long) reply_size);

	/* maybe save reply_buffer as well (where ??) ??? */

	append_to_statusfile (buf, &p_opt->where);
}
