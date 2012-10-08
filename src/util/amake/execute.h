/*	@(#)execute.h	1.3	94/04/07 14:49:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifdef __STDC__
#include <setjmp.h>
#endif

EXTERN char ContextString[];

enum JobState {
    J_INIT,	/* job has not started yet */
    J_READY,	/* job is on ReadyQ, or currently running */
    J_SUSPENDED	/* waiting for other jobs to finish */
};

#define J_INSERTED	0x01	/* job was inserted */

struct job {
    int 	   job_indicator;    /* job class number		    */
    generic 	   job_info;	     /* class dependent info		    */
    enum JobState  job_state;
    short	   job_flags;
    struct slist  *job_suspended;    /* jobs waiting for this one to finish */
    struct job    *job_last_susp;    /* the last job started up by this one */
    int 	   job_nr_waiting;   /* nr of jobs this one has started up  */
};

/* We could have added an slist containing pointers to the jobs it is waiting
 * for, but that causes overhead (find job on removal) and we don't need them.
 */

void ReSched		P((void));
void Catch		P((jmp_buf *buf ));
void EndCatch		P((void));
void FreeJob		P((struct job *job ));
void RemoveInsertedJob	P((struct job *prev ));
void PutInReadyQ	P((struct job *job ));
void SuspendCurrent	P((struct job *wait_for ));
void Wait		P((void));
void Awake		P((struct job *waiting , struct job *from ));
void StartExecution	P((struct job *until ));
void JobStructReport	P((void));
void ExecutionReport	P((void));
int  NewJobClass	P((char *name , void_func executer ,
			   void_func remover , string_func printer ));
int  JobIndicator	P((struct job *job ));
char *ExecContext	P((struct job *exec ));
char *JobTrace		P((void));
generic JobInfo		P((struct job *job ));

struct job *LastSuspendedOn	P((struct job *job ));
struct job *CreateJob		P((int class_no , generic ptr ));
struct job *InsertJob		P((struct job *job ));
struct job *LookupExec		P((struct slist *args ));
struct expr *exec_fn		P((struct slist *args ));

