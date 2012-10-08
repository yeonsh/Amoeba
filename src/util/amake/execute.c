/*	@(#)execute.c	1.3	96/02/27 13:06:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
#include "global.h"
#include "alloc.h"
#include "slist.h"
#include "docmd.h"
#include "expr.h"
#include "builtin.h"
#include "idf.h"
#include "dbug.h"
#include "error.h"
#include "eval.h"
#include "type.h"
#include "scope.h"
#include "derive.h"
#include "Lpars.h"
#include "objects.h"
#include "main.h"
#include "os.h"
#include "execute.h"

/* allocation definitions of struct job */
#include "structdef.h"
DEF_STRUCT(struct job, h_job, cnt_job)
#define new_job()	NEW_STRUCT(struct job, h_job, cnt_job, 20)
#define free_job(p)	FREE_STRUCT(struct job, h_job, p)

#define CHECK_JOBS	/* check that all jobs are finished at the end */

PUBLIC char ContextString[256];

#define MAX_CLASS 10			/* max number of job classes */

PUBLIC jmp_buf *SaveJump = NULL, *JumpTo = NULL;

PUBLIC struct job *CurrentJob = NULL;	/* The job we are now executing */
PRIVATE struct slist *ReadyQ;		/* Jobs to be executed */
PRIVATE int CommandsRunning = 0;	/* nr of cmds currenrly running */
PRIVATE struct slist *JobQ = NULL;
PRIVATE struct job *SaveCurrentJob = NULL;

PRIVATE void
ReSchedActions()
{
    CurrentJob->job_state = J_SUSPENDED;
    if (SaveCurrentJob != NULL) {
	CurrentJob = SaveCurrentJob;
    }
}

PUBLIC void
ReSched()
/* In the current implementation of simulated parallellism, jobs decide for
 * themselves when to suspend, which is done via a call to ReSched.
 */
{
    DBUG_ENTER("ReSched");
    ReSchedActions();
    DBUG_LONGJMP(JMPCAST(JumpTo), 1);
    /*NOTREACHED*/
}

PUBLIC void
Catch(buf)
jmp_buf *buf;
{
    assert(SaveJump == NULL);
    /* no nested catches allowed (although easy to implement) */
    SaveJump = JumpTo;
    JumpTo = buf;

    assert(SaveCurrentJob == NULL);
    SaveCurrentJob = CurrentJob;
}

PUBLIC void
EndCatch()
{
    JumpTo = SaveJump;
    SaveJump = NULL;

    assert(SaveCurrentJob == CurrentJob);
    SaveCurrentJob = NULL;
}

PUBLIC struct job *
LastSuspendedOn(job)
struct job *job;
{
    return(job->job_last_susp);
}


PUBLIC generic
JobInfo(job)
struct job *job;
{
    return(job->job_info);
}

struct class {
    char       *class_name;
    void_func   class_executer;
    void_func   class_remover;
    string_func class_printer;
} JobClass[MAX_CLASS];

#define good_class(class_no) ((class_no) >= 0 && (class_no) < MAX_CLASS)

PUBLIC struct job *
/*VARARGS1*/
CreateJob(class_no, ptr)
int class_no;
generic ptr;
{
    struct job *job;

    job = new_job();
    DBUG_PRINT("joblist", ("create job %lx of class %s",
			   job, JobClass[class_no]));
    job->job_state = J_INIT;
    job->job_indicator = class_no;
    job->job_info = ptr; /* type depends on class_no */
#ifdef CHECK_JOBS
    Append(&JobQ, job);
#endif
    return(job);
}

#ifdef DEBUG
PRIVATE void
check_class(class_no)
int class_no;
{
    if (!good_class(class_no)) {
	FatalError("illegal class nr.: %d", class_no);
    }
}
#else
#define check_class(class_no)
#endif

PUBLIC int
NewJobClass(name, executer, remover, printer)
char *name;
void_func executer, remover;
string_func printer;
{
    static int last_class = -1;

    ++last_class;
    check_class(last_class);

    JobClass[last_class].class_name = name;
    JobClass[last_class].class_executer = executer;
    JobClass[last_class].class_remover = remover;
    JobClass[last_class].class_printer = printer;

    return(last_class);
}

PRIVATE void
do_free_job(job)
struct job *job;
{
    register int job_class = job->job_indicator;

    check_class(job_class);

#ifdef CHECK_JOBS
{
    int found = FALSE;

    ITERATE(JobQ, job, j, {
	if (j == job) {
	    Remove(&JobQ, cons);
	    found = TRUE;
	    break;
	}
    });
    if (!found) {
	FatalError("job not in JobQ");
    }
}
#endif

    JobClass[job_class].class_remover(job->job_info);
    free_job(job);
    DBUG_PRINT("joblist", ("removed job %lx of class %s",
			   job, JobClass[job_class]));
}

PUBLIC void
FreeJob(job)
struct job *job;
/* only needed for failing exec jobs */
{
    do_free_job(job);
}

/* InsertJob and RemoveInsertedJob can be used by jobs to startup another job,
 * without being suspended.
 * The way to call it is:
 * (0)     Catch(..);
 * ...
 * (1)     new = CreateJob(Cl, info);
 * (2)     current = InsertJob(new); 
 * (3)     DoJobOfClassCl(&info);
 * (4)     RemoveInsertedJob(current);
 * ...
 * (5)     EndCatch(..);
 *
 * InsertJob takes care of adding the dependency of CurrentJob on the new job.
 * When the new job can be completed, this dependency is removed by
 * RemoveInsertedJob. Otherwise, the new job will have suspended itself
 * by a call to ReSched, where the previous Catch(..) has given information
 * with which job is to be continued. This job can then decide for itself
 * when to Wait() for jobs it started up.
 */
PUBLIC struct job *
InsertJob(job)
struct job *job;
{
    struct job *ret;

    DBUG_PRINT("suspend", ("InsertJob"));
    SuspendCurrent(job);
    ret = CurrentJob;
    CurrentJob = job;
    CurrentJob->job_flags |= J_INSERTED;
    return(ret);
}

PRIVATE void AwakeWaitingJobs( /* struct job *ready */ );

PUBLIC void
RemoveInsertedJob(prev)
struct job *prev;
{
    struct job *inserted = CurrentJob;
    register struct cons *cons;

    /* Remove the job that inserted `prev' from its job_suspended list,
     * without putting it on the ReadyQ: it already is there.
     */
    for (cons = Head(inserted->job_suspended); cons != NULL; cons = Next(cons))
    {
	if (ItemP(cons, job) == prev) {
	    Remove(&inserted->job_suspended, cons);
	    prev->job_nr_waiting--;
	    break;
	}
    }
    assert(cons != NULL);	  /* was removed */
    AwakeWaitingJobs(inserted);   /* now awake other jobs suspended on it !*/
    do_free_job(inserted);

    CurrentJob = prev;		  /* continue with previous job */
}

PRIVATE struct slist *ReadyExecs = NULL;
PRIVATE void DoJob( /* struct job *job */ );

PUBLIC char *
ExecContext(exec)
struct job *exec;
/* Returns a pointer to a (static) context string, describing what invoked
 * an exec. This is used when diagnostics are to be written on stdout.
 */
{
    extern int SingleTasking;
    struct job *orig;

    if (SingleTasking) { /* exec->job_suspended is not set in this case !! */
	orig = CurrentJob;
    } else {
	assert(Head(exec->job_suspended) == Tail(exec->job_suspended));
	orig = ItemP(HeadOf(exec->job_suspended), job);
    }

    check_class(orig->job_indicator);
    return(JobClass[orig->job_indicator].class_printer(orig->job_info));
}


PRIVATE char *
Spaces(i)
int i;
{
#   define MAX_DEPTH 10
    static char spaces[2*MAX_DEPTH+1] = "                    ";

    if (i > MAX_DEPTH) {
	i = MAX_DEPTH;
    }
    return(&spaces[2*MAX_DEPTH] - 2*i);
}

PRIVATE void
PrintTrace(job)
struct job *job;
{
    int job_class = job->job_indicator;
    static int trace_depth = 0;
    char *spc;

    if (!good_class(job_class)) {
	Message("job already deallocated\n");
	return;
    }

    spc = Spaces(trace_depth++);
    Message("%s%s\n", spc, JobClass[job_class].class_printer(job->job_info));

    if (!IsEmpty(job->job_suspended)) {
	/* only print one that is waiting for this job (recursively) */
	PrintTrace(ItemP(HeadOf(job->job_suspended), job));
    }

    trace_depth--;
}

PUBLIC char *
JobTrace()
{
    if (CurrentJob != NULL) {
	Message("Context:\n");
	PrintTrace(CurrentJob);
    }
}

PRIVATE void
HandleReadyCommand(exec)
struct job *exec;
/* A command has finished. In order to reduce memory usage, we may try to
 * continue (and hopefully: finish) the job who started it.
 */
{
    struct job *originator;

    DBUG_ENTER("HandleReadyCommand");
    assert(Head(exec->job_suspended) == Tail(exec->job_suspended));
    originator = ItemP(HeadOf(exec->job_suspended), job);

    CommandsRunning--;
    Append(&ReadyExecs, exec);

    /* only try inserted jobs, this avoids a lot of problems */
    if ((originator->job_flags & J_INSERTED) != 0) {
	originator->job_nr_waiting--;
	/* don't put in ReadyQ with AwakeWaitingJobs */
	CurrentJob = originator;
	DBUG_EXECUTE("ready", {
	    DBUG_PRINT("ready", ("inserted job:\n"));
	    PrintTrace(originator);
	});
	DoJob(originator);		/* try to finish it */
    } else {
	DBUG_EXECUTE("ready", {
	    DBUG_PRINT("ready", ("non inserted job:\n"));
	    PrintTrace(originator);
	});
	AwakeWaitingJobs(exec);		/* puts originator in ReadyQ */
    }
    DBUG_VOID_RETURN;
}

#ifdef DEBUG
    
PRIVATE void
do_print_job(job)
struct job *job;
{
    int job_class = job->job_indicator;
    static int depth = 0;
    char *spc;

    check_class(job->job_indicator);

    spc = Spaces(depth++);

    DBUG_PRINT("job", ("%s [%d] %s", spc, job->job_nr_waiting,
		       JobClass[job_class].class_printer(job->job_info)));

    ITERATE(job->job_suspended, job, j, {
	if (j != job) {
	    do_print_job(j);
	} else {
	    DBUG_PRINT("job", ("!CYCLE 2!"));
	}
    });

    depth--;
}

PRIVATE void
DoProcessDump()
{
    DBUG_PRINT("job", ("- start Job dump -"));
    DBUG_EXECUTE("job", { Iterate(ReadyQ, (Iterator) do_print_job); });
    DBUG_PRINT("job", ("-- end Job dump --"));
}

#define print_job(j)  do_print_job(j)
#define ProcessDump() DoProcessDump()
#else
#define print_job(j)
#define ProcessDump()
#endif

PUBLIC void
PutInReadyQ(job)
struct job *job;
{
#ifdef DEBUG
    if (job->job_state == J_READY) {
	int found = FALSE;

	/* it should be present in ReadyQ */
	ITERATE(ReadyQ, job, j, {
	    if (j == job) {
		found = TRUE;
		break;
	    }
	});
	if (!found) {
	    DBUG_PRINT("job", ("J_READY inconsistency:"));
	    print_job(job);
	}
    }
    /* put it in anyway, momentarily */
#endif
    Append(&ReadyQ, job);
    job->job_state = J_READY;
}

PRIVATE void
Suspend(job, wait_for)
struct job *job, *wait_for;
/* Let 'job' wait for 'wait_for' to be finished. 'job' must already be removed
 * from ReadyQ.
 */
{
    job->job_nr_waiting++;
    job->job_last_susp = wait_for;

    if (wait_for != NULL) {
	Append(&wait_for->job_suspended, job);
	DBUG_PRINT("job", ("sleep on job:"));
	print_job(wait_for);
    } else {
	DBUG_PRINT("job", ("sleep(0)"));
    }
}

PUBLIC void
SuspendCurrent(wait_for)
struct job *wait_for;
{
    Suspend(CurrentJob, wait_for);
}

/*
 * exec sub module.
 * Should not be put here.
 *
 * If a (background) command finishes, we put a ptr to the job running it into
 * ReadyExec. The job that called 'exec' is then re-evaluated.
 * On encountering the responsible exec-node, the result is computed from
 * the arguments (possibly coming from a RETURN node) and/or the exit status
 * of the command.
 * QUESTION: does this work OK with the current implementation of
 * tool-invokation ?
 */

PUBLIC struct job *
LookupExec(args)
struct slist *args;
{
    register struct cons *cons;

    for (cons = Head(ReadyExecs); cons != NULL; cons = Next(cons)) {
	struct job *ready = ItemP(cons, job);

	if (((struct process *)ready->job_info)->p_args == args) {
	    Remove(&ReadyExecs, cons);
	    return(ready);
	}
    }
    return(NULL);
}

PUBLIC struct expr *
exec_fn(args)
struct slist *args;
{
    int status;
    struct expr *res;
    struct job  *ready;

    DBUG_ENTER("exec_fn");
    if ((ready = LookupExec(args)) != NULL) {
	/* Re-evaluation of an expression containing a command that has
	 * just finished.
	 */
	struct process *p = (struct process *)ready->job_info;

	status = CommandStatus(p);
	res = ExecResult(args, (status == 0) ? TrueExpr : FalseExpr);
	RmCommand(p);
	do_free_job(ready);
    } else if (F_no_exec) {
	char *command, *redir;

	command = command_string(args, &redir);
	if (!F_silent) {
	    SafeMessage(command);
	    if (F_verbose) {		/* by default don't show redir */
		Message(redir);
	    }
	    Message("\n");
	}
	/* free diagnostic objects */
	HandleDiagnostics(args, "nocpu", CurrentJob);

	free(command); free(redir);
	res = ExecResult(args, TrueExpr);
    } else {
	struct job *job, *ready, *cur;
	
	if ((job = EnterCommand(CurrentJob, args, &status)) != NULL) {
	    CommandsRunning++;
	    DBUG_PRINT("suspend", ("wait for exec to finish"));
	    SuspendCurrent(job);

	    ReSchedActions();
	    cur = CurrentJob;

	    while ((ready = CommandGotReady()) != NULL) {
		HandleReadyCommand(ready);
	    }

	    CurrentJob = cur;
	    DBUG_LONGJMP(JMPCAST(JumpTo), 1);
	    /*NOTREACHED*/
	} else {
	    /* Command already finished (singletasking) or it couldn't be
	     * started. We can just finish the exec() call
	     */
	    res = ExecResult(args, (status == 0) ? TrueExpr : FalseExpr);
	}
    }
    DBUG_RETURN(res);
}

/*
 * Job scheduling routines.
 */

PUBLIC void
Wait()
{
    if (CurrentJob->job_nr_waiting > 0) {
	ReSched();
    }
}

/*ARGSUSED*/
PUBLIC void
Awake(waiting, from)
struct job *waiting, *from;
{
    assert(waiting->job_nr_waiting > 0);
    waiting->job_nr_waiting--;

    if (waiting->job_nr_waiting == 0) {
	PutInReadyQ(waiting);
    }
}

PRIVATE void
AwakeWaitingJobs(ready)
struct job *ready;
{
    register struct cons *cons;

    DBUG_ENTER("AwakeWaitingJobs");

    if (!IsEmpty(ready->job_suspended)) {
	for (cons = Head(ready->job_suspended); cons != NULL;) {
	    register struct cons *next = Next(cons);

	    Awake(ItemP(cons, job), ready);
	    Remove(&ready->job_suspended, cons);
	    cons = next;
	}
    }

    /* DBUG_EXECUTE("pdump", { ProcessDump(); }); */
    DBUG_VOID_RETURN;
}

PRIVATE void
DoJob(job)
struct job *job;
{
    int job_class = job->job_indicator;

    DBUG_ENTER("DoJob");
    check_class(job_class);
    print_job(job);
    JobClass[job_class].class_executer(&job->job_info);
    DBUG_PRINT("job", ("job could be completed"));
    AwakeWaitingJobs(job);
    do_free_job(job);
    DBUG_VOID_RETURN;
}

PRIVATE void
AwaitCommand()
{
    struct job *done;

    DBUG_ENTER("AwaitCommand");
    if ((done = AwaitCmd()) != NULL) {
	CommandsRunning--;
	Append(&ReadyExecs, done);
	AwakeWaitingJobs(done);
	/* DON'T throw it away yet, the status of done is needed */
    } else {
	FatalError("Expected child could not be found");
    }
    DBUG_VOID_RETURN;
}

PUBLIC void
StartExecution(until)
struct job *until;
/*
 * Execute jobs until 'until' is finished. If until == NULL continue until
 * all jobs are done.
 */
{
    static struct job *SaveStarted;
    jmp_buf ExecLoop;

    DBUG_ENTER("StartExecution");
    assert(JumpTo == NULL); /* no nested StartExecution()'s ? */
    JumpTo = (jmp_buf *)ExecLoop;
    switch(DBUG_SETJMP(ExecLoop)) {	/* should be: _setjmp */
    case 0:
	/* normal setjmp return */
	ProcessDump();
	break;
    default:
	DBUG_PRINT("setjmp", ("job couldn't be finished"));
	SaveStarted->job_state = J_SUSPENDED;
    }
    while (!IsEmpty(ReadyQ) || CommandsRunning) {
	CheckInterrupt();	/* don't let user wait too long */

	AttributeScopeNr = GLOBAL_SCOPE; /* re-init possibly necessary */
	/* DBUG_EXECUTE("pdump", { ProcessDump(); }); */
	if (!IsEmpty(ReadyQ)) {
	    SaveStarted = CurrentJob = RemoveHead(&ReadyQ, job);
	    DoJob(CurrentJob);

	    /* if we come here, the job could be finished */
	    if (CurrentJob == until) {
		DBUG_PRINT("job", ("job asked for is done"));
		break;
	    }
	    CurrentJob = NULL;
	} else {
	    AwaitCommand();
	}
    }
    JumpTo = NULL;
    DBUG_VOID_RETURN;
}

PUBLIC int
JobIndicator(job)
struct job *job;
{
    return(job->job_indicator);
}

PRIVATE void
Report(j)
struct job *j;
{
    Message("| %s ", JobClass[j->job_indicator].class_name);
    switch(j->job_state) {
    case J_INIT:      Message("(init)\n"); break;
    case J_READY:     Message("(ready)\n"); break;
    case J_SUSPENDED: Message("(suspended)\n"); break;
    }
    /* PrintTrace(j); */
    if (j->job_flags != 0) {
	Message("job_flags:      %d\n", j->job_flags);
    }
    if (j->job_nr_waiting != 0) {
	Message("job_nr_waiting: %d\n", j->job_nr_waiting);
    }
    if (!IsEmpty(j->job_suspended)) {
	Message("#job_suspended: %d\n", Length(j->job_suspended));
    }
}

#ifdef DEBUG
PUBLIC void
JobStructReport()
{
    StructReport("job", sizeof(struct job), cnt_job, h_job);
}
#endif

PUBLIC void
ExecutionReport()
{
#ifdef CHECK_JOBS
    if (!ErrorsOccurred && !IsEmpty(JobQ)) {
	Message("Job queue not empty:\n");
	ITERATE(JobQ, job, j, {
	    Report(j);
	});
    }
#endif
}

