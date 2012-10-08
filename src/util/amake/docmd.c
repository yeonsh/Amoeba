/*	@(#)docmd.c	1.4	96/02/27 13:06:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Parallel module for Amake.
 *
 * Inspired by:
 * parallel module for 'pmake' - author Erik baalbergen <erikb@cs.vu.nl>
 * Distributed version for Amoeba/Minix; Jan 1988
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "standlib.h"
#include "global.h"
#include "alloc.h"
#include "dbug.h"
#include "slist.h"
#include "error.h"
#include "execute.h"
#include "expr.h"
#include "builtin.h"
#include "objects.h"
#include "invoke.h"
#include "buffer.h"
#include "main.h"
#include "os.h"
#include "docmd.h"

#define P_EXPANDED 0x01
/* Flag set in process struct when argumentlist has been expanded */

/* 
 * Unless "-p 0"  specified as argument, we are multitasking by default.
 * Multitasking can be temporarily be avoided by calling DoSingleTask().
 * The default can be restored again by calling DoMultiTask().
 *
 * This feature is useful when it is difficult to suspend & restart
 * some algorithm that *might* (in rare circumstances) start a command.
 * This is the case for the command determination part of the
 * cluster algorithm, for example.
 */
PRIVATE int DefaultSingleTasking = FALSE;	/* default tasking state */
PUBLIC int SingleTasking = FALSE;		/* current tasking state */

PRIVATE struct process *CreateVproc P(( char *cpu ));

PUBLIC void
DoSingleTask()
{
    SingleTasking = TRUE;
}

PUBLIC void
DoMultiTask()
{
    /* restore to default */
    SingleTasking = DefaultSingleTasking;
}

/* lists on which the commands are kept */
PRIVATE struct slist
    *Running = NULL,	/* until we get wait() returns its pid */
    *ToDo = NULL,	/* the OS doesn't have room for these, until now */
    *Finished = NULL,	/* results not delivered yet */
    *IdleVprocs = NULL; /* only the p_cpu field matters here */

/*
 * Scheduling can easily be done better. Someday.
 * Idea: give each cpu a number of 'quanta' and let a command run on the
 * one with most quanta available. The idea is to keep all processors
 * busy up to some 'load avarage'. Of course we should take user-time history
 * into account, and let large jobs take more than one quantum.
 */

#ifdef USE_RSH

/*	toargv() - turns a null-terminated string into an argument vector
 *	Author: Erik Baalbergen Date: Oct 1, 1985
 */
PRIVATE int
toargv(text, sep, argv, argvsize)
	char *text, *sep, **argv;
	int argvsize;
{
	int argc = 0;

	if (text == 0)
		return 0;
	while (*text && strchr(sep, *text))
		text++;
	while (*text) {
		argv[argc++] = text;
		if (argc >= argvsize)
			return argc;
		while (*text && !strchr(sep, *text))
			text++;
		if (*text)
			*text++ = '\0';
		while (*text && strchr(sep, *text))
			text++;
	}
	if (argc < argvsize)
		argv[argc] = 0;
	return argc;
}

#endif /* USE_RSH */

PRIVATE struct process *
CreateVproc(cpu)
char *cpu;
{
    register struct process *vproc = new_process();

    vproc->p_cpu = cpu;
    vproc->p_state = P_IDLE;
    return(vproc);
}

#define LEN 22

PRIVATE char *
tail_of(s)
char *s;
{
    int len = strlen(s);

    return(LEN < len ? s + len - LEN : s);
}

PRIVATE char *
head_of(s)
char *s;
{
    static char HeadString[LEN+1] = {'\0'};
    int n = strlen(s);

    strncpy(HeadString, s, LEN);
    if (n < LEN) HeadString[n] = '\0';
    return(HeadString);
}

PRIVATE void
ExpandCommandLine(proc)
struct process *proc;
{
    assert(proc->p_args != NULL);
    if ((proc->p_flags & P_EXPANDED) == 0) {
	proc->p_cmd = command_string(proc->p_args, &proc->p_redir);
	proc->p_flags |= P_EXPANDED;
    }
}

PRIVATE void
print_process(proc, prefix)
struct process *proc;
char *prefix;
{
    Message("%s ", prefix);
    switch (proc->p_state) {
    case P_RUN:
	Message("RUN  (%d@%s)\t%s .. %s\n", proc->p_pid, proc->p_cpu,
		head_of(proc->p_cmd), tail_of(proc->p_cmd));
	break;
    case P_TODO:
	ExpandCommandLine(proc);
	Message("TODO (prio=%ld)\t%s .. %s\n", proc->p_prio,
		head_of(proc->p_cmd), tail_of(proc->p_cmd));
	break;
    case P_DONE:
	Message("DONE (%x)\t@%s\t%s .. %s\n", proc->p_status, proc->p_cpu,
		head_of(proc->p_cmd), tail_of(proc->p_cmd));
	break;
    case P_IDLE:
	Message("IDLE\t%s\n", proc->p_cpu); break;
    default:
	CaseError("print_process: %d", (int)proc->p_state);
    }
}

PRIVATE void
DUMP(proc_list, prefix)
struct slist *proc_list;
char *prefix;
{
    ITERATE(proc_list, process, p, {
	print_process(p, prefix);
    });
}

PRIVATE void
ProcessDump()
{
    Message("Process dump:\n");
    DUMP(Running,    "[R]");
    DUMP(ToDo,	     "[T]");
    DUMP(Finished,   "[F]");
    DUMP(IdleVprocs, "[I]");
    Message("--------------\n");
}

#define NPROC_MAX 200	/* maximum number of commands run in parallel */
#define NPROC_DEF 4	/* default number */

#if NPROC_DEF > NPROC_MAX
!!!!READ THIS!!!! NPROC_DEF should be less than or equal to NPROC_MAX
#endif

PRIVATE int nproc = NPROC_DEF;

#define POOL "RSH_POOL"

#ifndef STD_POOL
/* STD_POOL can be redefined at compile time, or in the environment */
#define STD_POOL	"draak pinas pampus loper"
#endif

PRIVATE char *
Cpu(j)
int j;
{
    char cpu[3];

    assert(j < 100);
    (void) sprintf(cpu, "%d", j);
    return(Salloc(cpu, (unsigned)((j <= 9) ? 2 : 3)));
}
    
PRIVATE void
CreateLocalPool()
{
    register int i;

    for (i = 0; i < nproc; i++) {
	Append(&IdleVprocs, CreateVproc(Cpu(i)));
    }
}

PUBLIC void
init_distr()
{
#ifdef USE_RSH
    char *physproc[NPROC_MAX];
    register int i, size;

    if (F_use_rsh) {
	if ((size = toargv(getenv(POOL), " \t\n", physproc, NPROC_MAX)) != 0 ||
	    (size = toargv(STD_POOL, " ", physproc, NPROC_MAX)) != 0) {

	    /* assign virtual processors to physical processors */
	    for (i = 0; i < nproc; i++) {
		Append(&IdleVprocs, CreateVproc(physproc[i % size]));
	    }
	} else {
	    Warning("empty pool of remote machines, no remote execution");
	    F_use_rsh = FALSE;
	    CreateLocalPool();
	}
    } else {
	CreateLocalPool();
    }
#else
    CreateLocalPool();
#endif
}

PUBLIC void
RenameProcessors(proc)
struct process *proc;
/* For some reason remote execution doesn't work after all. Rename the
 * processors to "0", .., "<nproc - 1>". 
 */
{
    int n = 0;

    proc->p_cpu = Cpu(n++);
    ITERATE(IdleVprocs, process, p, {
	p->p_cpu = Cpu(n++);
    });
}

PRIVATE void
WriteCommand(proc)
struct process *proc;
{
    if (!F_silent) {
#ifdef DEBUG
	Message("[%d@%s] ", proc->p_pid, proc->p_cpu);
#else
	Message("[%s] ", proc->p_cpu);
#endif
	SafeMessage(proc->p_cmd); /* `Message' will core with long command */
	if (F_verbose) {
	    Message(proc->p_redir);
	}
	Message("\n");
    }
}

#define MAX_FORK_TRY  5  /* give up after MAX_FORK_TRY + nproc failed forks */
#define FORK_SLEEP    5  /* sleep a few seconds after a failed fork, when
			  * only one vproc is left
			  */
PRIVATE void
ForkFailed(vproc)
char *vproc;
{
    /* We have a vproc but we cannot fork (possibly out of memory).
     * Putting the command at the end of ToDo doesn't help a bit, because
     * forking (Amake) is causing the trouble, not execing.
     * What we will do is destroy vproc, because its presence apparently
     * assumes too much memory on the machine.
     */
    static int forks_tried = 0;

    if (nproc > 1) {
	nproc--;
	Warning("fork failed, removing vproc %s", vproc);
    } else if (forks_tried++ < MAX_FORK_TRY) {
	Warning("fork failed, keep trying");
	Append(&IdleVprocs, CreateVproc(vproc));
	/* Give the system a chance to quiet down a bit.
	 * Alternatively, the user may kill some of its other processes
	 * (some UNIX systems have a per user process limit of 20).
	 */
	sleep(FORK_SLEEP);
    } else {
	FatalError("fork failed, give up after %d tries", MAX_FORK_TRY);
    }
}

PRIVATE struct process *
RemoveProcess(list, pid)
struct slist **list;
int pid;
/* If a process with pid 'pid' occurs in 'list', remove it from the list
 * and return it. Otherwise return NULL.
 */
{
    ITERATE(*list, process, p, {
	if (p->p_pid == pid) {
	    Remove(list, cons);
	    return(p);
	}
    });

    /* not found */
    return(NULL);
}

PRIVATE void
CouldntStart(proc)
struct process *proc;
{
    proc->p_state = P_DONE;
    proc->p_status = 13;
}

PRIVATE char *
ProcessFinished(pid, status)
int pid, status;
{
    register struct process *done;

    if ((done = RemoveProcess(&Running, pid)) != NULL) {
	if (status == 0) {
	    DBUG_PRINT("wait", ("done [%d@%s]: `%s'",
				pid, done->p_cpu, done->p_cmd));
	} else {
	    Verbose(1, "process %d at [%s] exited with status %d",
		    pid, done->p_cpu, status);
	}
	done->p_state = P_DONE;
	done->p_status = status;
	Append(&Finished, done);

	HandleDiagnostics(done->p_args, done->p_cpu, done->p_job);
	return(done->p_cpu);
    } else {
	Warning("wait() delivered unexpected command (pid = %d)", pid);
	return(NULL);
    }
}

PRIVATE int pwait();

PRIVATE void
AdministerCommand(proc, pid)
struct process *proc;
int pid;
{
    proc->p_pid = pid;
    proc->p_state = P_RUN;
    WriteCommand(proc);
    Append(&Running, proc);
    if (SingleTasking) {
	int wait_pid;

	/* Though SingleTasking is *currently* true, we still might have
	 * started up multiple commands in the past. Therefore we wait
	 * explicitly until the process `proc' is ready.
	 */
	do {
	    wait_pid = pwait();
	} while (wait_pid >= 0 && wait_pid != pid);

	if (wait_pid != pid) {
	    FatalError("could not find process while singletasking");
	}
    }
}

PRIVATE void PrioInsert();

PRIVATE void
ReSchedule(vproc)
char *vproc;
{
    int pid;
    struct process *todo;

    todo = RemoveHead(&ToDo, process);
    todo->p_cpu = vproc;
    ExpandCommandLine(todo);
    if ((pid = ForkOff(todo)) >= 0) {
	AdministerCommand(todo, pid);
    } else {
	if (pid == FORK_AGAIN) {
	    ForkFailed(vproc);
	    PrioInsert(&ToDo, todo);
	} else {
	    CouldntStart(todo);
	    Append(&Finished, todo); /* don't retry */
	    Append(&IdleVprocs, CreateVproc(vproc));
	}
    }
}

PUBLIC struct job *
CommandGotReady()
/* Should only be called after an EnterCommand(). This can be used to
 * finish a few jobs, hanging on finished commands.
 */
{
    if (!IsEmpty(Finished)) {
	return((RemoveHead(&Finished, process))->p_job);
    } else {
	return(NULL);
    }
}

PRIVATE int
pwait()
/*
 * Wait for a command to finish and return TRUE iff this succeeds.
 * The command is added to the Finished list. It's vproc is reused, by
 * starting a command from ToDo if available, or added to IdleVprocs.
 */
{
    char *vproc;
    int pid, status = 0;

    do {
	if ((pid = BlockingWait(&status)) < 0) {
	    CheckInterrupt(); /* failure might have been caused by interrupt */
	    DBUG_PRINT("wait", ("no (more) children!"));
	    return(-1);
	}
	vproc = ProcessFinished(pid, status);
    } while (vproc == NULL);

    CheckInterrupt();

    if (!IsEmpty(ToDo) && !ErrorsOccurred && !SingleTasking) {
	/* let the new process use the now-free vproc done->p_cpu */
	ReSchedule(vproc);
    } else {
	/* We currently have no use for the vproc. */
	Append(&IdleVprocs, CreateVproc(vproc));
    }
    return(pid);
}

#define DONT_TRY	4

PRIVATE int
FinishedInMeanTime()
{
    char *vproc;
    int pid;
    int status = 0;
    int found = FALSE;
    static int dont_try = 0;

    /* To avoid continous request `has anything finished?' we only
     * do a nonblocking wait when either the last call succeeded,
     * or when this function has been tried (unsuccesfully) DONT_TRY times,
     * since the last failing call.
     */
    if (dont_try-- > 0) {
	return(FALSE);
    }

    while (!IsEmpty(Running) && (pid = NonBlockingWait(&status)) >= 0) {
	DBUG_PRINT("wait", ("child finished in the meantime"));
	if ((vproc = ProcessFinished(pid, status)) != NULL) {
	    Append(&IdleVprocs, CreateVproc(vproc));
	    found = TRUE;
	}
    }
    
    dont_try = found ? 0 : DONT_TRY;
    return(found);
}

PUBLIC void
AwaitProcesses()
{
    DBUG_PRINT("wait", ("wait for completion of outstanding commands"));

    ErrorsOccurred++; /* don't start new commands */
    while (pwait() >= 0) {}
}

PUBLIC int
SetParallelism(n)
int n;
{
    if (n == 0) {
	nproc = 1;
	/* There is one processor, but Amake blocks when it has forked
	 * one process off
	 */
	SingleTasking = DefaultSingleTasking = TRUE;
    } else if ((1 <= n) && (n <= NPROC_MAX)) {
	nproc = n;
    } else {
	PrintError("-p <par>: <par> should be 0 or in range [1..%d]\n",
		   NPROC_MAX);
	return(FALSE);
    }
    return(TRUE);
}

PUBLIC void
par_opt(s)
char *s;
{
    /* -Pn<m>: allow <m> simultaneous processes besides Amake */

    switch (*s) {
    case 'n':
	break;
#ifdef USE_RSH
    case 'r': /* remote */
	F_use_rsh = TRUE;
	break;
    case 'l': /* local */
	F_use_rsh = FALSE;
	break;
#endif
    default:
	Message("illegal option: -P%s\n", s);
	Message("-Pn<m>:allow <m> simultanious processes\n");
#ifdef USE_RSH
	Message("-Pr   :use rsh to spawn commands on other machines\n");
	Message("       chosen from environment var RSH_POOL, if present\n");
#endif
	exit(EXIT_FAILURE);
    }
}

PRIVATE void
TryJobsToDo()
{
    while (!IsEmpty(ToDo) && (!IsEmpty(IdleVprocs) || FinishedInMeanTime())) {
	register struct process *vp = RemoveHead(&IdleVprocs, process);
	register char *cpu = vp->p_cpu;

	free_process(vp);
	ReSchedule(cpu);
    }
}

PUBLIC int
CommandStatus(proc)
struct process *proc;
{
    return(proc->p_status);
}

#ifdef DEBUG
PRIVATE int CommandAdded = FALSE;
#endif

PUBLIC struct job *
AwaitCmd()
{
    register struct process *done;

    DBUG_EXECUTE("pdump", {
	if (CommandAdded) {
	    ProcessDump();
	    CommandAdded = FALSE;
	}
    });
    if (IsEmpty(Finished)) {
	TryJobsToDo();
	if (!IsEmpty(Running) && pwait() < 0) {
	    ProcessDump();
	    FatalError("wait() failed while processes were running");
	}
    } 

    /* deliver the first finished cmd */
    assert(!IsEmpty(Finished));
    done = RemoveHead(&Finished, process);
    return(done->p_job);
}

/*
 * Link with Job module.
 */

PRIVATE int ClassCommand;

PRIVATE void
PrioInsert(proclist, proc)
struct slist **proclist;
struct process *proc;
/* Scheduling based on input size: do (expectedly) large jobs first */
{
    extern int F_scheduling;

#ifdef DEBUG
    CommandAdded = TRUE;
#endif
    if (F_scheduling) {
	ITERATE(*proclist, process, p, {
	    if (p->p_prio <= proc->p_prio) {
		/* first one with less or equal priority */
		Insert(proclist, cons, (generic) proc);
		return;
	    }
	});
    }

    /* not inserted, so Append it */
    Append(proclist, proc);
}

PRIVATE void
TryLater(execing, proc)
struct job *execing;
struct process *proc;
{
    DBUG_PRINT("vproc", ("no vproc left"));
    proc->p_state = P_TODO;
    proc->p_cpu = "NoCpu";
    proc->p_prio = InputSize(execing);
    PrioInsert(&ToDo, proc);
}

enum fork_result { forked, dont_try, try_later };

PRIVATE enum fork_result
TryToExecute(execing, proc)
struct job *execing;
struct process *proc;
/* delivers FALSE if there is no sense in trying it again */
{
    int pid;
    register struct process *vproc;

    /* Even when SingleTasking is TRUE, in the past commands may have
     * been started up in the background.  Sadly enough, this also means 
     * that all our Vprocs may be in use currently.
     * If so, we'll just have to wait for the first background job
     * to finish, and take its vproc.
     */
    if (SingleTasking && IsEmpty(IdleVprocs)) {
	if (pwait() < 0) {
	    FatalError("wait() for running processes failed\n");
	}
	assert(!IsEmpty(IdleVprocs));	/* this is what we needed */
    }

    if (!IsEmpty(IdleVprocs) || FinishedInMeanTime()) {
	ExpandCommandLine(proc);
	vproc = RemoveHead(&IdleVprocs, process);
	proc->p_cpu = vproc->p_cpu;
	free_process(vproc);
	if ((pid = ForkOff(proc)) >= 0) {
	    AdministerCommand(proc, pid);
	    return(forked);
	} else {
	    if (pid == FORK_AGAIN && !SingleTasking) {
		/* apparently too much vprocs */
		ForkFailed(proc->p_cpu);
		TryLater(execing, proc);
		return(try_later);
	    } else {
		/* serious problem with this command, don't try again */
		CouldntStart(proc);
		Append(&IdleVprocs, CreateVproc(proc->p_cpu));
		return(dont_try);
	    }
	}
    } else { /* all (virtual) processors busy */
	TryLater(execing, proc);
	return(try_later);
    }
}

PUBLIC struct job *
EnterCommand(execing, args, statusp)
struct job *execing;
struct slist *args;
int *statusp;
{
    struct job *job;
    struct process *proc;

    CheckInterrupt();

    proc = new_process();
    proc->p_args = args;
    job = CreateJob(ClassCommand, (generic)proc);
    proc->p_job = job;

    switch (TryToExecute(execing, proc)) {
    case forked:
	if (SingleTasking) { 
	    struct process *done = RemoveProcess(&Finished, proc->p_pid);

	    assert(done == proc);
	    *statusp = proc->p_status;	/* it has finished already */
	    RmCommand(proc); FreeJob(job); job = NULL;
	}
	break;
    case dont_try:
	*statusp = 13;			/* fake bad status */
	RmCommand(proc); FreeJob(job); job = NULL;
	break;
    default:
	break;
    }
    return(job);
}

/*ARGSUSED*/
PRIVATE void
DoCommand(proc)
/* shouldn't be called by the job sceduler */
struct process **proc;
{
    FatalError("DoExecute: process appeared in ReadyQ");
}

PUBLIC void
RmCommand(proc)
struct process *proc;
{
    /* Has to be called by the execute module directly, after the exec-result
     * has been constructed.
     */
    free(proc->p_cmd);
    free(proc->p_redir);
    free_process(proc);
}

/*ARGSUSED*/
PRIVATE void
FakeRmCommand(proc)
struct process *proc;
{
}

#include "Lpars.h"

PRIVATE char *
PrCommand(proc)
struct process *proc;
{
    struct expr *prog = ItemP(HeadOf(proc->p_args), expr)->e_right;

    if (prog->e_number == OBJECT) {
	if ((proc->p_flags & P_EXPANDED) != 0) {
	    (void)sprintf(ContextString, "execution of `%s', [%d@%s]",
			 SystemName(prog->e_object), proc->p_pid,proc->p_cpu);
	} else {
	    (void)sprintf(ContextString, "hanging execution of `%s'",
			 SystemName(prog->e_object));
	}
	return(ContextString);
    } else {
	return("unexpanded execution");
    }
}

PUBLIC void
InitCommand()
{
    ClassCommand = NewJobClass("command", (void_func) DoCommand, 
			   (void_func) FakeRmCommand, (string_func) PrCommand);
}

PUBLIC void
CommandReport()
{
    if (!IsEmpty(Running)) {
	Warning("#Running:  %d\n", Length(Running));
    }
    if (!IsEmpty(ToDo)) {
	Warning("#ToDo:     %d\n", Length(ToDo));
    }
    if (!IsEmpty(Finished)) {
	Warning("#Finished: %d\n", Length(Finished));
    }
}
