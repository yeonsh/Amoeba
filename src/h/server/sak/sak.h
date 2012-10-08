/*	@(#)sak.h	1.2	94/04/06 17:05:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SVR_SAK_H__
#define __SVR_SAK_H__

#define	MAX_NAME_LEN	32

struct caplist;

struct sak_job_options {
	capability	where;			/* capability of
						 * user status file
						 */
	int8		save_result;		/* add message to user status
						 * file upon completion
						 */
	int8		catchup;		/* when server notices that
						 * the specified time has 
						 * pasted without running this
						 * job it will run it
						 * immediately when cathup =
						 * TRUE
						 */
	char		name[MAX_NAME_LEN];	/* name of job */
};

#define	MINU		0
#define	HOUR		1
#define	MDAY		2
#define	MONTH		3
#define	WDAY		4
#define	YEAR		5
#define	MAX_SPEC	(YEAR + 1)

#define	NONE		(objnum) 0
#define	ANY		1
#define	VALUE		2
#define	LIST		3
#define	RANGE		4
#define	MAX_SPEC_SIZE	62		/* one for type, one DELIM. and max
					 * of 60 values in a list makes 62
					 */

#define DELIMITER	-1		/* used to as delimiter in schedule
					 * lists
					 */

#define	MAX_ARGS	128
#define	MAX_ENVS	64
#define	MAX_CAPS	64

#define	SAK_SUBMIT_JOB		((SAK_FIRST_COM))
#define	SAK_LIST_JOB		((SAK_FIRST_COM) + 1)
#define	SAK_EXEC_FILE		((SAK_FIRST_COM) + 2)
#ifdef DEBUG
#define	SAK_LISTJOBLIST		((SAK_FIRST_COM) + 3)
#define	SAK_LISTRUNLIST		((SAK_FIRST_COM) + 4)
#endif /* DEBUG */

#define	SAK_TOOLATE		((SAK_FIRST_ERR))
#define	SAK_ARGTOOBIG		((SAK_FIRST_ERR) - 1)
#define	SAK_ENVTOOBIG		((SAK_FIRST_ERR) - 2)

/* used for execfile transaction buffer size */
#define	EXECFILE_BUFSIZE	10240

/* Server stubs */
errstat sak_exec_trans
	_ARGS((char **, char **, struct caplist *, capability *));

errstat sak_list_job _ARGS((capability *, int8 **, struct sak_job_options *));

errstat sak_make_transfile
	_ARGS((header *, char *, bufsize, bufsize, interval, capability *));

errstat sak_submit_job
	_ARGS((char *, int8 **, capability *, struct sak_job_options *,
								capability *));



#endif /* __SVR_SAK_H__*/
