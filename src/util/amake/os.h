/*	@(#)os.h	1.4	96/02/27 13:06:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */


/* ForkOff returns childpid (>= 0) or one of the following */
#define FORK_AGAIN	(-1)	/* fork failed, but caller may try again */
#define FORK_FAILURE	(-2)	/* fork failed, no sense in trying again */

/* Define STR_ID if value-ids should be represented as strings, internally.
 * This is necessary when they don't fit in a long; capabilities for example.
 */
#ifdef STR_ID
#define NO_ID		"':0'"
#define NO_VALUE_ID	":0"
#else
#define NO_ID		"0"
#define NO_VALUE_ID	0
#endif

#define SHELL_METAS	"#|^();&<>*?[]:$\"\\`'"

/* At a few strategic places Amake looks, by calling CheckInterrupt, whether
 * the user has asked for a premature exit.  Doing anything other than setting
 * a flag in the signal handler is not safe, in general.
 */
#ifdef __STDC__
EXTERN volatile int Interrupted;
#else
EXTERN int Interrupted;
#endif
#define CheckInterrupt()	if (Interrupted) { PrematureExit(); }

EXTERN struct expr *NoValueId; /* value id for non existing file */

void PrematureExit	P((void));
void IgnoreUserSignals	P((void));
void InitOS		P((void));
void DoDelete		P((char *filename , enum fatal_or_not fatal ));
void MakePrivateDir	P((struct object *dir ));
void CleanupPrivateDirs P((void));
long GetObjSize		P((struct object *obj ));
long GetFileSize	P((char *filename));
int  DoLink		P((char *existing_path , char *new_path ));
int  BlockingWait	P((int *statusp ));
int  NonBlockingWait	P((int *statusp ));
int  ForkOff		P((struct process *proc ));
int  Writable		P((struct object *obj ));
int  FilesEqual		P((struct object *new , struct object *old ));

struct expr *GetValueId P((struct object *obj ));
struct expr *GetValueId P((struct object *obj ));

