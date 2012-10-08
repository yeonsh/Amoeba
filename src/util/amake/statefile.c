/*	@(#)statefile.c	1.5	94/04/07 14:54:20 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <assert.h>
#include <stdio.h>
#include <stdio.h>
#include "global.h"
#include "unistand.h"
#include "alloc.h"
#include "idf.h"
#include "Lpars.h"
#include "error.h"
#include "objects.h"
#include "expr.h"
#include "type.h"
#include "caching.h"
#include "dump.h"
#include "dbug.h"
#include "builtin.h"
#include "scope.h"
#include "slist.h"
#include "assignment.h"
#include "main.h"
#include "cluster.h"
#include "lexan.h"
#include "invoke.h"
#include "execute.h"
#include "symbol2str.h"
#include "os.h"
#include "conversion.h"
#include "docmd.h"
#include "statefile.h"

/*
 * Ideas for more state information, so that we can recognise quicker that
 * nothing has to be done. This could be done by including cluster
 * source/target bindings, e.g.
 *
 * CLUSTER name
 * {
 * TARGETS foo[cid = 'X12345'];
 * SOURCES file1[cid = 'X123'], file2[cid = 'X456'];
 * }
 *
 * However, most of the time it happens that a fraction of the previous
 * commands has to be redone, instead of none at all. In order to make
 * this most common phase as fast as possible, we should make more use
 * of the info we already have, and extend it where necessary.
 * What we have is a cache with previous commands. What we don't know is
 * where they were used for. Currently, in each amake run we start the
 * `tool invocation determination algorithm' all over, and see which commands
 * have to be redone. Supposing the Amake description file hasn't changed,
 * (which can be check by putting the ID's of it, together with its recursive
 *  includes, in the Statefile), and the relevant attributes of the objects
 * haven't changed, we will get the same list of invocations as in a
 * previous run. (The attributes can change if they are determined
 * dynamicly, by invoking tools. Until now this feature isn't used much.)
 *
 * Currently, the cache contains (apart from normal assignments, that
 * are used to reduce the statefile's size) entries of the form
 * _= tool_name(arg1, arg2, .., argn) == result_expr;
 *
 * We could replace result_expr by an appropriate tuple, which also contains
 * information about the cluster(s) that used it, and when they used it.
 * For example:
 *
 * _= tool1(arglist) == {res1, used_by(clus1, 1)};
 * _= tool2(arglist) == {res2, used_by(clus2, 1), used_by(clus3, 4)};
 * _= tool3(arglist) == {res3, used_by(clus1, 2)};
 *
 * 'used_by' is an internal function, which attaches its arguments to
 * the cache entry it appears in. Alternatively we could refer to cache
 * entries via the (numeric) index in the CACHE list, and have a seperate
 * USAGE entry:
 *
 * USAGE = {
 *     {1, clus1, 1},
 *     {2, clus2, 2},
 *     {2, clus3, 4},
 *     {3, clus1, 2},
 * ..
 * };
 *
 *
 * The amount of redundancy that appears in the Statefile is reduced as
 * follows. The idea is to give a number of list-argument and object
 * definitions, which can be referred to in the following cache enries. This
 * way we can avoid duplicating long pathnames of objects appearing more than
 * once. Another source of redundancy is the repetition of [cid = X123]
 * attribute assignments. This can be done by adding them to the assignments
 * just described.
 *
 * Object assignments are of the form:
 * 	_<number> = SystemName(obj) [cid = <cid>];
 *
 * e.g.:
 * _0=/usr/proj/em/Work/modules/h/alloc.h[cid = 123];
 * _1=/usr/proj/em/Work/modules/h/assert.h[cid = 456];
 *
 * To avoid having to repeat the (in Amoeba usually very long) pathnames
 * of the parents, they are `factored out' also, so instead we will generate
 *
 * _0=/usr/proj/em/Work/modules/h;
 * _1=$_0/alloc.h[cid = 123];
 * _2=$_0/assert.h[cid = 456];
 */

PUBLIC int ReadingCache = FALSE;
/* Checked in Eval. If it is TRUE, attributes are put at scope
 * CacheScopeNr instead of GLOBAL_SCOPE.
 */

/* When we are recovering from a previous system break-down, we start
 * generating the new Statefile a bit sooner, to avoid losing information
 * when the system breaks down again.
 */
PUBLIC int Recovering = FALSE;

PRIVATE int Dumping = FALSE;
/* set to TRUE iff we are dumping the Statefile after a signal */

PUBLIC struct idf *Id_cache_entry;
/* assignments to identifier "_" add to the cache */

#define WriteTok(token)	Write(symbol2str(token))

PRIVATE char *
SharedVarName(index)
int index;
{
    static char shared_var[10];

    (void)sprintf(shared_var, "_opt%d", index);
    return(shared_var);
}

PRIVATE void
WriteObjectVar(obj)
struct object *obj;
{
    static char var_string[6];

    (void)sprintf(var_string, "_%d", obj->ob_index);
    Write(var_string);
}

PRIVATE void
WriteParent(parent)
struct object *parent;
{
    if (parent != RootDir && parent != Context &&
	(parent->ob_flags & O_DUMPED) == 0) {
	/* generate assignment for a parent object in order to avoid
	 * repetituous long pathnames in the cache.
	 */
	WriteObjectVar(parent); Write("=");
	PrExpr(ObjectExpr(parent)); Write(";"); NL();
	parent->ob_flags |= O_DUMPED;
    }	
}

PRIVATE void
WriteObjectName(obj)
struct object *obj;
{
    register struct object *parent = obj->ob_parent;
	
    if (parent != Context && (parent->ob_flags & O_DUMPED) != 0) {
	Write("$"); WriteObjectVar(parent); Write("/");
	Write(QuotedId(obj->ob_idp->id_text));
    } else {
	PrExpr(obj->ob_expr);
    }
}

PUBLIC struct slist *UsedSharedExpressions = NULL;

struct sharedexpr {
    struct expr *sh_expr;
    short	 sh_index;	/* assigned variable number */
    short	 sh_dumped;	/* is it already dumped? */
};
    
PUBLIC void
SharedExpressionUsed(expr)
struct expr *expr;
/* The shared expression `expr' is used in a cached invocation.
 * Put it on a list whose entries we will dump to the statefile before the
 * first cache entry using it.
 */
{
    static int shared_index = 0;
    struct sharedexpr *shared;

    ITERATE(UsedSharedExpressions, sharedexpr, sh, {
	if (sh->sh_expr == expr) {
	    return;
	}
    });
    DBUG_EXECUTE("sharedexpr", {
	DBUG_WRITE(("shared expression used |"));
	DBUG_Expr(expr); DBUG_WRITE(("|")); DBUG_NL();
    });
    shared = (struct sharedexpr *)Malloc(sizeof(struct sharedexpr));
    shared->sh_expr = expr;
    shared->sh_index = shared_index++;
    shared->sh_dumped = FALSE;
    Append(&UsedSharedExpressions, shared);
}

PRIVATE void
GenSharedLists()
{
    /* Dump the shared lists, which are used in tool invocations.
     * Note that it is prossibly wise not to do this for all shared lists,
     * because only a few of them will generally be used as default argument
     * (the most important one is probably CFLAGS, currently).
     * So what we do is this: when we evaluate a tool default, we look
     * if it's a expression. If so, we put it on the `used shared expressions'
     * list. Before we are going to dump the cache entries (i.e.: now) we
     * generate variable definitions for them, which are going to be referred
     * to from the cache entries. This can be done by simple lookup.
     */
    ITERATE(UsedSharedExpressions, sharedexpr, sh, {
	if (!sh->sh_dumped) {
	    Write(SharedVarName(sh->sh_index)); Write("=");
	    PrExpr(sh->sh_expr); Write(";"); NL();
	    sh->sh_dumped = TRUE;
	}
    });
}

PRIVATE void
WriteAttributes(obj)
struct object *obj;
{
    register struct expr *val;

    Write("[");
    Write(Id_cid->id_text); Write("=");
    if ((val = GetAttrInScope(obj, Id_cid, CacheScopeNr))!=UnKnown ||
	(val = GetAttrInScope(obj, Id_cid, GLOBAL_SCOPE))!=UnKnown) {
#ifndef STR_ID
	val = GetInteger(val);
#endif
	PrExpr(val);
    } else {
	/* Always write a cid attribute, even if it is not currently present.
	 * Not doing so has caused problems in the past.
	 */
	Write(NO_ID);
    }

    Write("]");
}

PRIVATE void
WriteExpr(e)
struct expr *e;
/* Write an expression, but take possibility of object (requiring attribute
 * assignment) into account.
 */
{
    register struct expr *cid;

    if (e->e_number == OBJECT) {
	if ((e->e_object->ob_flags & O_DUMPED) != 0) {
	    Write("$"); WriteObjectVar(e->e_object);
	    /* The cid attribute has already been set globally in assignment,
	     * but the tool using the object may have seen a different one.
	     * In that case the cid attribute has been set locally.
	     */
	    if ((cid = GetAttrInScope(e->e_object, Id_cid, CacheScopeNr))
		!= UnKnown) {
		Write("[");
		Write(Id_cid->id_text); Write("=");
#ifndef STR_ID		
		cid = GetInteger(cid);
#endif
		PrExpr(cid);
		Write("]");
	    }
	} else {
	    WriteObjectName(e->e_object);
	    WriteAttributes(e->e_object);
	}
    } else {
	PrExpr(e);
    }
}


PRIVATE void flush_buf();

PRIVATE void
WriteShared(e)
struct expr *e;
{
    ITERATE(UsedSharedExpressions, sharedexpr, sh, {
	if (sh->sh_expr == e) {
	    Write("$"); Write(SharedVarName(sh->sh_index));
	    return;
	}
    });
    flush_buf();
    FatalError("shared variable not found");
}

PRIVATE void
WriteInvocation(inv)
struct expr *inv;
{
    register struct cons *acons;

    Indent();
    Write(inv->e_func->id_text);
    Write("(");
    NL();
    Incr();
    for (acons = Head(inv->e_args); acons != NULL; acons = Next(acons)) {
	struct expr *arrow = ItemP(acons, expr);
	
	Indent();
	if (IsList(arrow->e_right)) {
	    /* a shared object list is not written as variable ref, because
	     * some of its components might have gotten a different `cid'
	     * in the meantime
	     */
	    if (IsShared(arrow->e_right) && 
		((arrow->e_right->e_type & T_OBJECT) == 0)) {
		WriteShared(arrow->e_right);
	    } else {
		Write("{");
		ITERATE(arrow->e_right->e_list, expr, elem, {
		    WriteExpr(elem);
		    if (Next(cons) != NULL) Write(",");
		});
		Write("}");
	    }
	} else {
	    WriteExpr(arrow->e_right);
	}
	if (Next(acons) != NULL) Write(",");
	NL();
    }
    Decr();
    Indent();
    Write(")");
}

PRIVATE struct object *Statefile, *NewState;
PRIVATE char *StateName, *NewStateName;
PRIVATE int StateExists = FALSE;

#ifdef AMOEBA
#include <amoeba.h>
#include <ampolicy.h>
#include <cmdreg.h>
#include <stderr.h>
#include <server/bullet/bullet.h>
#include <module/stdcmd.h>
#include <module/name.h>

PRIVATE capability bulletcap;
PRIVATE capability filecap, newfilecap;

#else

PRIVATE FILE *Statefilp;

#endif

#define BUFFER_SIZE	4096

PRIVATE char
    out_buf[BUFFER_SIZE],
    *buf_tail = &out_buf[BUFFER_SIZE-1],
    *buf_ptr = out_buf;

#ifdef AMOEBA
#define size_type b_fsize
#else
#define size_type int
#endif

PRIVATE void
flush_buf()
{
    static size_type bytes_written = 0;
    size_type bytes = buf_ptr - out_buf;
#ifdef AMOEBA
    errstat status;
    char *bulldir = DEF_BULLETSVR;

    /* On Amoeba, we want to make sure that we don't leave an uncommitted
     * (bullet)statefile behind, because when then loose everything if the
     * the cpu running amake crashes. Therefore, we cannot use stdio
     * but instead use committed bulletfile updates.
     */
    if (bytes_written == 0) {
	/* Find the capability for the bullet server */
	if ((status = name_lookup(bulldir, &bulletcap)) != STD_OK) {
	    FatalError("can't access bullet server %s (%s)\n",
		       bulldir, err_why(status));
	}

	/* Create initial statefile */
	if ((status = b_create(&bulletcap, out_buf, bytes,
			       BS_COMMIT | BS_SAFETY, &filecap)) != STD_OK) {
	    FatalError("can't create statefile (%s)\n", err_why(status));
	}

	/* and install it */
	if ((status = name_append(NewStateName, &filecap)) != STD_OK) {
	    if (status == STD_EXISTS) {
		status = name_replace(NewStateName, &filecap);
	    }
	    if (status != STD_OK) {
		FatalError("couldn't install statefile (%s)", err_why(status));
	    }
	}
    } else {
	/* add out_buf to the current version */
	if ((status = b_modify(&filecap, bytes_written, out_buf, bytes,
			       BS_COMMIT | BS_SAFETY, &newfilecap)) != STD_OK){
	    FatalError("couldn't modify statefile (%s)", err_why(status));
	}
	/* install this new version */
	if ((status = name_replace(NewStateName, &newfilecap)) != STD_OK){
	    FatalError("couldn't replace statefile (%s)", err_why(status));
	}
	/* throw old version away */
	if ((status = std_destroy(&filecap)) != STD_OK) {
	    Warning("couldn't destroy old statefile (%s)", err_why(status));
	}
	/* new current version */
	filecap = newfilecap;
    }
#else
    if (bytes_written == 0) {
	if ((Statefilp = fopen(NewStateName, "w")) == NULL) {
	    FatalError("couldn't open `%s' for writing", NewStateName);
	}
    }
    if ((bytes > 0) && (fwrite(out_buf, (int)bytes, 1, Statefilp) < 1)) {
	FatalError("couldn't write to statefile");
    }
#endif
    buf_ptr = out_buf; /* reset */
    bytes_written += bytes;
}    

PRIVATE void
WriteState(s)
char *s;
{
    register int len = strlen(s);

    assert(len < BUFFER_SIZE);
    if (buf_tail - buf_ptr < len) {	/* doesn't fit */
	flush_buf();
    }
    (void) strcpy(buf_ptr, s);
    buf_ptr += len;
}

PRIVATE WriterFunc save_writer = NULL;

PRIVATE void
StartWriting()
{
    save_writer = Write;
    Write = WriteState;
}

PRIVATE void
StopWriting()
{
    flush_buf();
    Write = save_writer;
    save_writer = NULL;
}

PRIVATE void
CommitStatefile()
{
#ifdef AMOEBA
    errstat status;

    /* the statefile is already committed, we just have to install it */
    if (!StateExists) {
	if ((status = name_append(StateName, &filecap)) != STD_OK) {
	    Warning("couldn't install new statefile (%s)", err_why(status));
	}
    } else if ((status = name_replace(StateName, &filecap)) != STD_OK) {
	Warning("couldn't replace statefile (%s)", err_why(status));
    }
    if ((status = name_delete(SystemName(NewState))) != STD_OK) {
	Warning("couldn't delete %s", SystemName(NewState));
    }
#else
    fclose(Statefilp);
    if (StateExists) {
	DoDelete(StateName, NON_FATAL); /* or make a backup? */ 
    }
    rename(NewStateName, StateName);
#endif
}

PUBLIC void
CheckStatefile()
{
    if (!Writable(Statefile)) {
	Warning("statefile (%s) cannot be written to", SystemName(Statefile));
    }
}

PUBLIC void
ReadStateFile()
{
    /* evaluate hanging assignments */
    StartExecution((struct job *)NULL);

    ReadingCache = TRUE;

    if (access(NewStateName, R_OK) == 0) {
	Recovering = TRUE;
	Warning("recovering results of previous run");
    }

    if (access(StateName, R_OK) == 0) {
	/* read statefile if available */
	StateExists = TRUE;
	read_from(StateName);
	LLstatefile();
    }

    if (Recovering) {
	/* Recover results of previous run.
	 * A cache entry with the same first input object is added in
	 * front of others, the most recent ones should be added last.
	 */
	F_one_version = TRUE;	/* to get rid of double cache entries */
	read_from(NewStateName);
	LLstatefile();
    }

    F_reading = FALSE; /* no more line numbers before messages */

    /* evaluate old-style statefile variables */
    StartExecution((struct job *)NULL);

    CacheCommit();
    ReadingCache = FALSE;

    /* The used variant directories are now known because they are mentioned
     * the statefile.
     */
    InitVariants();
}

PRIVATE void
DumpObjectDef(obj)
struct object *obj;
{
    struct expr *cid;

    if ((obj->ob_flags & O_DUMPED) == 0 &&
	(SourceOfSomeCluster(obj) ||
	 GetAttrValue(obj, Id_implicit) != UnKnown ||
	 obj->ob_parent == AmakeDir))
    {
	WriteParent(obj->ob_parent);
	WriteObjectVar(obj); Write("=");
	WriteObjectName(obj);
	Write("[");
	Write(Id_cid->id_text); Write("=");
	if ((cid = GetAttrInScope(obj, Id_cid, GLOBAL_SCOPE)) != UnKnown ||
	    (cid = GetValueId(obj)) != UnKnown) {
#ifndef STR_ID
	    cid = GetInteger(cid);
#endif
	    PrExpr(cid);
	} else {
	    Write(NO_ID);
	}
	Write("];"); NL();
	obj->ob_flags |= O_DUMPED;
    }
}

PRIVATE void
DumpAssignments(c)
struct cached *c;
{
    register struct cons *cns;
    
    for (cns = Head(c->c_invocation->e_args); cns != NULL; cns = Next(cns)) {
	struct expr *arrow = ItemP(cns, expr);
	register int type = arrow->e_left->e_param->par_type->tp_indicator;

	if ((type & T_IN) != 0) {
	    if ((type & T_LIST_OF) != 0) {
		ITERATE(arrow->e_right->e_list, expr, obj_expr, {
		    DumpObjectDef(obj_expr->e_object);
		});
	    } else {
		DumpObjectDef(arrow->e_right->e_object);
	    }
	} else if ((type & T_OUT) != 0 && (type & T_LIST_OF) == 0) {
	    WriteParent(arrow->e_right->e_object->ob_parent);
	    /* a version directory */
	}
    }
}

#define CACHE_LIMIT	5 /* dump every CACHE_LIMIT new entries */
PRIVATE struct slist *CacheList = NULL;

PRIVATE void
DoDumpCacheEntry(c)
struct cached *c;
{
    CacheScopeNr = c->c_scope_nr;
    DumpAssignments(c);
    Write("_=");
    WriteInvocation(c->c_invocation);
    WriteTok(EQ);
    PrExpr(c->c_result);
    Write(";"); NL();
}

PUBLIC void
DumpCacheEntry(c)
struct cached *c;
{
    static int in_cache_list = 0;
    register struct cons *cns, *next;

    if (F_no_exec || (c->c_flags & CH_PRESENTED) != 0) {
	return;
    } else {
	c->c_flags |= CH_PRESENTED;

	Append(&CacheList, c);
	if (++in_cache_list < CACHE_LIMIT) {
	    return; /* wait until there are CACHE_LIMIT entries to be dumped */
	} else {
	    StartWriting();
	    GenSharedLists();
	    for (cns = Head(CacheList); cns != NULL; cns = next) {
		struct cached *c = ItemP(cns, cached);

		next = Next(cns);
		DoDumpCacheEntry(c);
		c->c_flags |= CH_DUMPED;
		Remove(&CacheList, cns);
	    }
	    StopWriting();
	    in_cache_list = 0;
	}
    }
}

PRIVATE void
DoUpdate()
{
    register struct cached *c;

    DBUG_EXECUTE("objreport", { 
	if (!Dumping) {
	    register struct object *obj;

	    for (obj = FirstObject; obj != NULL; obj = obj->ob_next) {
		PrintObject(obj);
	    }
	}
    });

    if (F_cleanup) {
	/* Remove the outputs of all cached invocations, and the Statefile.
	 * This now happens in main() by just calling "rm -rf .Amake".
	 * The reason for this is that things might have slipped into .Amake
	 * that amake doesn't know about.
	 */
	return;
    }

    if (F_one_version && Interrupted) {
	/* Safety messure: we don't know for sure which invocations
	 * can be thrown away. Therefore just switch to normal mode.
	 */
	F_one_version = FALSE;
    }

    /* It isn't always necessary to write a new statefile.
     * - if we aren't executing in this run, the cache is unchanged
     * - if the cache is unchanged, and the user didn't explicitly ask
     *   to keep just one version, the Statefile will still be up to date.
     */
    if (F_no_exec || (!F_one_version && !CacheChanged)) {
	CleanupPrivateDirs();	/* although it may be empty alreadt */
	DBUG_PRINT("state", ("cache unchanged"));
	return;
    }

    if (Dumping) {
	Message("dumping cache..");
    }

    IgnoreUserSignals();	/* no truncated Statefile, please */

    StartWriting();
    GenSharedLists();
    /* Invocations not used in this run should only be removed from the
     * cache if the user explicitly asked for it, and we haven't
     * been interrupted.
     */
    for (c = FirstCacheEntry; c != NULL; c = c->c_next) {
	if ((c->c_flags & CH_DUMPED) == 0) {
	    if (RetainCacheEntry(c)) {
		DoDumpCacheEntry(c);
		c->c_flags |= CH_DUMPED;
	    } else {
		RemoveOutputs(c->c_invocation);
	    }
	}
    }
    StopWriting();
    CommitStatefile();

    if (Dumping) {
	Message("\n");
    }
}

PUBLIC void
DumpStatefile()
/* dump statefile without relying on the job mechanism, because we have
 * to do it immediately.
 */
{
    if (save_writer == NULL && !Dumping) {
	/* avoid FatalError upon FatalError while dumping/writing statefile */
	if (!F_no_exec) {
	    if (FirstCacheEntry != NULL) {
		Dumping = TRUE;
#ifdef AMOEBA
		Message("wait for commands still running...\n");
		AwaitProcesses();
#endif
		DoUpdate();
	    } else {
		CleanupPrivateDirs();
	    }
	}
    }
}

PUBLIC void
UpdateStateFile()
{
    DoUpdate();
}

PUBLIC void
InitUpdate()
{
    register char *name;

    MakePrivateDir(AmakeDir);	/* if it doesn't yet exist */

    Id_cache_entry = str2idf("_", 0);
    Id_cache_entry->id_flags |= F_QUIET;

    Statefile = ObjectWithinParent(str2idf("Statefile", 0), AmakeDir);
    name = SystemName(Statefile);
    StateName = Salloc(name, (unsigned)(strlen(name) + 1));

    NewState = ObjectWithinParent(str2idf("Statefile.new", 0), AmakeDir);
    name = SystemName(NewState);
    NewStateName = Salloc(name, (unsigned)(strlen(name) + 1));
}

