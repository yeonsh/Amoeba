/*	@(#)docmd.h	1.2	94/04/07 14:48:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

enum PState {
	P_RUN,   /* currently running (or not yet waited for) */
	P_TODO,  /* couldn't be executed immediately (lack of vprocs) */
	P_DONE,  /* finished cmd whose results haven't been delivered yet */
	P_IDLE   /* an idle virtual processor */
};

/*
 * struct process describes a command delivered to this module.
 */
struct process {
    struct job   *p_job;	/* so that the job can be found back easily */
    short	  p_flags;
    enum PState	  p_state;	/* process state */
    char         *p_cmd;	/* full specification of the command */
    char  	 *p_redir;	/* /bin/sh redirection string */
    struct slist *p_args;	/* used by execute module to find it back */
    int           p_status;    	/* status delivered by wait() */
    int	 	  p_pid;	/* process identifier */
    char         *p_cpu; 	/* name of the virtual processor it runs on */
    long	  p_prio;	/* input size is used as priority */
};

/* allocation definitions of struct process */
#include "structdef.h"
DEF_STRUCT(struct process, h_process, cnt_process)
#define new_process()    NEW_STRUCT(struct process, h_process, cnt_process, 10)
#define free_process(p)  FREE_STRUCT(struct process, h_process, p)

void DoSingleTask	P((void));
void DoMultiTask	P((void));
void init_distr		P((void));
void RenameProcessors	P((struct process *proc ));
void AwaitProcesses	P((void));
void par_opt		P((char *s ));
void RmCommand		P((struct process *proc ));
void InitCommand	P((void));
void CommandReport	P((void));
int  SetParallelism	P((int n ));
int  CommandStatus	P((struct process *proc ));

struct job *AwaitCmd	 P((void));
struct job *EnterCommand P((struct job *execing, struct slist *args, int *stp));
struct job *CommandGotReady P((void));

