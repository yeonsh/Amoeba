/*	@(#)sak_exec.c	1.4	96/02/27 10:18:51 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include	<stdio.h>
#include	"saksvr.h"
#include	"capset.h"
#include	"caplist.h"
#include	"module/proc.h"


errstat sak_exec (buf, size)
char *buf;
bufsize size;
{
	char *p;
	errstat err;
	extern char **environ;
	extern struct caplist *capv;
	char **sak_strenv, **u_argv, **u_strenv;
	struct caplist *sak_capenv, *u_capenv;
	interval save_timeout;
	capability execcap;
	static mutex exclude_exec;

	p = buf;

	err = STD_OK;

	p = buf_get_argvptr (p, buf + size, &u_argv, MAX_ARGS, &err);
	if (err != STD_OK)
		return (err);

	p = buf_get_argvptr (p, buf + size, &u_strenv, MAX_ENVS, &err);
	if (err != STD_OK)
		return (err == SAK_ARGTOOBIG ? SAK_ENVTOOBIG : err);

	p = buf_get_caplptr (p, buf + size, &u_capenv, MAX_CAPS, &err);
	if (p == (char *) 0)
	{
		if (err == STD_OK)
			return (STD_ARGBAD);
		return (err);
	}

#ifdef DEBUG
	if (debug > 2)
	{
		int i;

		printf ("Executing : %s", u_argv[0]);
		for (i = 1; u_argv[i] != (char *) 0; i++)
			printf (" '%s'", u_argv[i]);
		printf ("\n");
	}
#endif
	/* set string & capability environment to users specified env
	 * DON'T let them use server environment.
	 */

	/* save server environment */
	mu_lock (&exclude_exec);
	sak_strenv = environ;
	sak_capenv = capv;

	/* set user environment */
	environ = u_strenv;
	capv = u_capenv;

	save_timeout = timeout (10 * SEC);

	if ((err = name_lookup (u_argv[0], &execcap)) == STD_OK)
		err = exec_file (&execcap, NILCAP, NILCAP, 0, u_argv + 1,
				(char **) 0, (struct caplist *) 0, NILCAP);

	timeout (save_timeout);

	/* reset server environment */
	environ = sak_strenv;
	capv = sak_capenv;

	mu_unlock (&exclude_exec);

	buf_free_argvptr (u_argv);
	buf_free_argvptr (u_strenv);
	buf_free_caplptr (u_capenv);

	return err;
}

void dosak_exec ()
{
	header hdr;
	bufsize req_bufsize, reply_bufsize;
	char transbuf[EXECFILE_BUFSIZE];
	extern capability priv_exec_cap; /* the port we listen to! */

	for (;;) {
		for (;;) { /* loop until valid req */
			hdr.h_port = priv_exec_cap.cap_port;
			req_bufsize = getreq (&hdr, transbuf, EXECFILE_BUFSIZE);

			if (ERR_STATUS(req_bufsize))
				fprintf (stderr,
				  "sak: dosak_exec getreq failed : %s\n",
				  err_why ((errstat) ERR_CONVERT(req_bufsize)));
			else
				break;
		}

		reply_bufsize = 0;

		switch (hdr.h_command) {
		case STD_INFO:
			(void) strcpy (transbuf, "Sak exec file server");
			hdr.h_size = reply_bufsize = strlen (transbuf);
			hdr.h_status = STD_OK;
			break;
		case STD_RESTRICT:
			hdr.h_status = STD_OK;
			break;
		case STD_AGE:
			hdr.h_status = STD_OK;
			break;
		case STD_TOUCH:
			hdr.h_status = STD_OK;
			break;
		case SAK_EXEC_FILE:
			hdr.h_status = sak_exec (transbuf, req_bufsize);
			break;
		default:
			hdr.h_status = STD_COMBAD;
			break;
		}

		putrep (&hdr, transbuf, reply_bufsize);
	}
}
