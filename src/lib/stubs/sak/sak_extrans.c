/*	@(#)sak_extrans.c	1.2	94/04/07 11:07:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include	"sak/saksvr.h"

#ifdef __STDC__
errstat sak_exec_trans (char **argv, char **envp, struct caplist *capl,
							    capability *ptcap)
#else
errstat sak_exec_trans (argv, envp, capl, ptcap)
char **argv;
char **envp;
struct caplist *capl;
capability *ptcap;
#endif /* __STDC__ */
{
	char *p;
	header hdr;
	errstat	err;
	char *buffer;
	capability execcap;

	if ((buffer = (char *) malloc (EXECFILE_BUFSIZE)) == (char *) 0) {
		fprintf (stderr, "Couldn't alloc %u bytes\n", 
							EXECFILE_BUFSIZE);
		return (STD_NOMEM);
	}

	p = buffer;
	p = buf_put_argvptr (p, buffer + EXECFILE_BUFSIZE, argv);
	p = buf_put_argvptr (p, buffer + EXECFILE_BUFSIZE, envp);
	p = buf_put_caplptr (p, buffer + EXECFILE_BUFSIZE, capl);
	if (p == (char *) 0)
	{
		fprintf (stderr,
			"string + capability environment to big (> %d bytes)\n",
			EXECFILE_BUFSIZE);
		free (buffer);
		return (STD_NOMEM);
	}
	err = name_lookup (DEF_EXECSVR, &execcap);
	if (err != STD_OK)
	{
		fprintf (stderr, "Can't find execution server : %s\n",
					err_why (err));
		free (buffer);
		return (err);
	}

	hdr.h_port = execcap.cap_port;
	hdr.h_priv = execcap.cap_priv;
	hdr.h_command = SAK_EXEC_FILE;

	err = sak_make_transfile (&hdr, buffer, p - buffer,
							0, 10 * 1000, ptcap);

	free (buffer);
	return (err);
}
