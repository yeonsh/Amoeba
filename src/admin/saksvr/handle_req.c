/*	@(#)handle_req.c	1.4	94/04/06 11:53:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include	<stdio.h>
#include	"saksvr.h"

/* verify will check to see if the header contains valid information.
 * if it is valid it will touch the job (reset it's age)
 * privptr	in	private part of cap
 * prights	out	right bits substracted from priv part if
 *			capability is valid
 * prnd		out	random number of job if capability valid
 * return		valid/not valid
 */

static int8 verify_and_touch (privptr, prights, prnd)
private *privptr;
rights_bits *prights;
port *prnd;
{
	struct sak_job *pjob;

	if (prv_number (privptr) == GENERATIC)
		*prnd = superrnd;
	else {
		mu_lock (&job_list_sem);
			pjob = lookup (prv_number (privptr));
			if (pjob != NILCELL) {
				*prnd = pjob->rnd;
			/* job is used so touch it regardless off request */
				pjob->age = MAXAGE;
			}

		mu_unlock (&job_list_sem);

		if (pjob == NILCELL)
			return (FALSE);
	}

	if (prv_decode (privptr, prights, prnd) != 0)
		return (FALSE);
	else
		return (TRUE);
}


static int nr_threads_in_req = 0;

static mutex exclude_req_threads;

/* keep track of # threads waiting in request, if there are to many
 * already tell thread to return to pool opf free threads
 */
static int8 enter_getreq ()
{
	int8 stop_now = FALSE;

	mu_lock (&exclude_req_threads);
		if (nr_threads_in_req >= NR_OF_REQ_THREADS)
			stop_now = TRUE;
		else
			++nr_threads_in_req;
	mu_unlock (&exclude_req_threads);

	return (stop_now);
}

/* keep track of # threads waiting in req if none left, try (but don't force)
 * to alloc a free thread to service requests
 */
static void leave_getreq ()
{
	mu_lock (&exclude_req_threads);
		if (--nr_threads_in_req == 0)
			/* don't force allocation of new thread
			 * just see if one happens to be free
			 */
			(void) alloc_free_thread (NONE, REQ_TYPE);
	mu_unlock (&exclude_req_threads);
}

/* get request/handle it/reply
 */
void handle_req ()
{
	header hdr;
	rights_bits rights;
	bufsize req_bufsize, reply_bufsize;
	objnum jobnr, newjobnr;
	char transbuf[TRANSBUFSIZE];
	errstat err;
	port rnd;

	for (;;) {
		if (enter_getreq ())
			return;

#ifdef DEBUG
		if (debug > 3) printf ("Req Thread entering getreq\n");
#endif
		for (;;) { /* loop until valid req */
			hdr.h_port = supercap.cap_port;
			req_bufsize = getreq (&hdr, transbuf, TRANSBUFSIZE);

			if (ERR_STATUS(req_bufsize))
				fprintf (stderr, "getreq failed : %s\n",
				err_why ((errstat) ERR_CONVERT(req_bufsize)));
			else
				break;
		}
#ifdef DEBUG
		if (debug > 3) printf ("Req Thread received request\n");
#endif

		leave_getreq ();

		reply_bufsize = 0;

		if (verify_and_touch (&hdr.h_priv, &rights, &rnd) == FALSE) {
			hdr.h_status = STD_CAPBAD;
			MON_EVENT ("invalid cap in req");
		} else {
			jobnr = prv_number (&hdr.h_priv);

			switch (hdr.h_command) {
			case STD_INFO:
				MON_EVENT ("info request");
				err = sak_std_info (jobnr, rights, transbuf,
						    &reply_bufsize);
				hdr.h_status = err;
				hdr.h_size = reply_bufsize;
				break;
			case STD_RESTRICT:
				MON_EVENT ("restrict request");
				rights &= (rights_bits) hdr.h_offset;
				(void) prv_encode (&hdr.h_priv, jobnr, rights,
						   &rnd);

				hdr.h_status = STD_OK;
				break;
			case STD_AGE:
				MON_EVENT ("age jobs request");
				err = sak_std_age (jobnr, rights);
				hdr.h_status = err;
				break;
			case STD_TOUCH:
				MON_EVENT ("touch job request");
				/* nothing to do already touched */
				hdr.h_status = STD_OK;
				break;
			case STD_DESTROY:
				MON_EVENT ("destroy job request");
				err = sak_std_destroy (jobnr, rights);
				hdr.h_status = err;
				break;
			case SAK_SUBMIT_JOB:
				MON_EVENT ("submit job request");
				err = sak_submitjob (jobnr, transbuf,
						      req_bufsize,
						      &newjobnr, &rnd);

				if (err == STD_OK)
					(void) prv_encode (&hdr.h_priv,
							   newjobnr,
							   OWNERRIGHTS,
							   &rnd);

				hdr.h_status = err;
				break;
			case SAK_LIST_JOB:
				MON_EVENT ("list job request");
				err = sak_listjob (jobnr, rights, transbuf,
						TRANSBUFSIZE, &reply_bufsize);

				hdr.h_status = err;
				break;
#ifdef DEBUG
			case SAK_LISTRUNLIST:
				(void) debug_list_runlist ();
				hdr.h_status = STD_OK;
				break;
			case SAK_LISTJOBLIST:
				(void) debug_list_joblist ();
				hdr.h_status = STD_OK;
				break;
#endif
			default:
				MON_EVENT ("bad command request");

				hdr.h_status = STD_COMBAD;
				break;
			}

			if (hdr.h_status != STD_OK)
				MON_EVENT ("request failed");
		}

		putrep (&hdr, transbuf, reply_bufsize);
	}
}

/* find job struct in list
 * assumes a locked list !!
 * if found return pointer to struct else retun NILCELL
 * objnr	in	jobnr to find
 * return		ptr to job struct if found else NILCELL
 */
struct sak_job *lookup (objnr)
objnum objnr;
{
	struct sak_job *ptr;

	ptr = joblist[hashval (objnr)];
	while (ptr != NILCELL && ptr->ident <= objnr) {
		if (ptr->ident == objnr)
			return (ptr);
		ptr = ptr->next;
	}

	return (NILCELL);
}

/* age jobs 
 * if to old delete it
 */
void agejobs ()
{
	int i;
	struct sak_job *pjob;
	objnum jobnr;

#ifdef DEBUG
	if (debug > 1) printf ("Starting garbage collection\n");
#endif
	mu_lock (&job_list_sem);
	for (i = 0; i < MODVAL; i++)
		for (pjob = joblist[i]; pjob != NILCELL; pjob = pjob->next)
			pjob->age--;

	for (i = 0; i < MODVAL; i++)
		for (pjob = joblist[i]; pjob != NILCELL;)
			if (pjob->age == 0) {
				jobnr = pjob->ident;
				mu_unlock (&job_list_sem);
				remove_job (jobnr, TRUE);
				mu_lock (&job_list_sem);
				/* list has changed rescan current list */
				pjob = joblist[i];
			}
			else
				pjob = pjob->next;

	mu_unlock (&job_list_sem);

#ifdef DEBUG
	if (debug > 1) printf ("Finished garbage collection\n");
#endif
}
