/*	@(#)assignment.c	1.4	94/04/07 14:46:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <assert.h>
#include <stdio.h>
#include "global.h"
#include "standlib.h"
#include "alloc.h"
#include "dbug.h"
#include "idf.h"
#include "expr.h"
#include "error.h"
#include "eval.h"
#include "lexan.h"
#include "execute.h"
#include "slist.h"
#include "type.h"
#include "builtin.h"
#include "buffer.h"
#include "conversion.h"
#include "scope.h"
#include "statefile.h"
#include "caching.h"
#include "assignment.h"
#include "input.h"

PRIVATE int ClassAssignment;
PUBLIC struct idf *Id_dummy_var;

PRIVATE char *strsub		P(( char *str, char *sub ));
PRIVATE struct expr *EnvToAmake P(( char *val, char *sep ));
PRIVATE void PutEnv		P(( struct idf *idp, char *env_string ));
PRIVATE void AddAmakeValueToBuf	P(( struct expr *val, char *sep ));
PRIVATE void PutInEnvironment	P(( struct idf *idp ));
PRIVATE char *PrAssignment	P(( struct variable *var_def ));
PRIVATE void RmAssignment	P(( struct variable *var_def ));

PUBLIC void
DoInsertText(txt)
char *txt;
{
    int len = strlen(txt);

    if (InsertText(txt, len) != 1) {
	FatalError("couldn't insert %d characters", len);
    }
}

/* In order to handle things like "amake -Daap=noot -D'mies=$aap'" correctly,
 * the pieces of text have to be inserted in backward order.
 */

PRIVATE struct slist *CmdLineDefs = NULL;

PUBLIC void
CmdLineDefine(def)
char *def;
{
    Append(&CmdLineDefs, def);
}

PUBLIC void
ExecuteCmdLineDefs()
{
    register struct cons *cons;

    file_info.f_file = "commandline";
    for (cons = Tail(CmdLineDefs); cons != NULL; cons = Prev(cons)) {
	char *def = (char *)Item(cons);

	DoInsertText(def);
	LLcmdline();
    }
}

/*
 * Environment handling: import a variable from the environment as Amake
 * variable. Environmentstrings containing the per-variable list separator
 * (default ":", as in /bin/sh $PATH) are converted to lists.
 *
 * An exported Amake variable is converted to environment string, using the
 * list separator if its value is a list.
 */

PRIVATE char *
strsub(str, sub)
char *str, *sub;
/* return first nonempty substring `sub' in `str', or NULL if not available */
{
    register char *s;
    register int n = strlen(sub);

    for (s = strchr(str, *sub); s != NULL; s = strchr(s + 1, *sub)) {
	/* the first character matches, now compare the whole substring */
	if (strncmp(s, sub, n) == 0) {
	    return(s);
	}
    }
    return(NULL);
}

PRIVATE struct expr *
EnvToAmake(val, sep)
char *val;
char *sep;
{
    /* Create an Amake expression from an environment value, i.e., a STRING
     * or STRING LIST.
     */
    register char *sp_ptr, *ptr = val;
    register struct expr *list = NULL;
    register int sep_len = strlen(sep);

    while ((sp_ptr = strsub(ptr, sep)) != NULL) {
	if (list == NULL) {
	    (list = empty_list())->e_type = T_LIST_OF|T_STRING;
	}

	*sp_ptr = '\0'; 			/* terminate this component */
	Append(&list->e_list, StringExpr(ptr));
	*sp_ptr = *sep;  			/* restore */
	ptr = sp_ptr + sep_len; 
    }

    if (list == NULL) { /* single string */
	return(StringExpr(ptr));
    } else {
	Append(&list->e_list, StringExpr(ptr)); /* add last component */
	return(list);
    }
}

#define ENV_INIT	30
#define ENV_EXTRA	10
PRIVATE int EnvSize, EnvLeft;
PUBLIC char **Environment;

PRIVATE void
PutEnv(idp, env_string)
struct idf *idp;
char *env_string;
/* assume there is no putenv() */
{
    assert(strchr(env_string, '=') != NULL);

    if ((idp->id_variable->var_flags & V_IN_ENV) != 0) {
	/* replace an old 'var=value' string */
	/* no free(*envp); because it may be an original evironment string */
	Environment[idp->id_variable->var_index] = env_string;
	return;
    }

    /* not yet present, add to environment */
    idp->id_variable->var_flags |= V_IN_ENV;

    if (EnvLeft == 0) {
	Environment = (char **)Realloc((char*)Environment,
			  (unsigned) ((EnvSize + ENV_EXTRA) * sizeof(char*)));
	EnvLeft += ENV_EXTRA;
    }
    EnvLeft--;
    idp->id_variable->var_index = EnvSize;
    Environment[EnvSize++] = env_string;
    Environment[EnvSize] = NULL;
}

PUBLIC void
ReadEnvironment(env)
char *env[];
/*
 * Converts all environment variables to Amake variables. As ANSI C doesn't
 * have putenv(), we have to make a copy of the environment which we can
 * extend it ourselves. The copy is made here.
 */
{
    register int i;

    for (i = 0; env[i] != NULL; i++) {
	register char *eq_ptr = strchr(env[i], '=');
	struct idf *idp;

	assert(eq_ptr != NULL); /* should contain an '=' */
	*eq_ptr = '\0';		/* terminate variable */
	idp = str2idf(env[i], 1);
	DeclareIdent(&idp, I_VARIABLE);

#ifdef IMPORT_ALL
	EnterVarDef(idp, EnvToAmake(eq_ptr + 1, ELEM_SEPERATOR));
	idp->id_variable->var_flags |= V_FROM_ENV; /* avoid putting it twice */
#endif

	*eq_ptr = '=';		/* restore */

	PutEnv(idp, env[i]); /* copy the pointer in our own environment */
    }
}

PRIVATE void
AddAmakeValueToBuf(val, sep)
struct expr *val;
char *sep;
{
    if ((val->e_type & T_LIST_OF) != 0) {
	ITERATE(val->e_list, expr, elem, {
	    struct expr *str = GetExprOfType(CopyExpr(elem), T_STRING);

	    BufferAddString(do_get_string(str));
	    put_expr(str);
	    if (Next(cons) != NULL) {
		BufferAddString(sep);
	    }
	});
    } else {
	struct expr *str = GetExprOfType(CopyExpr(val), T_STRING);
		
	BufferAddString(do_get_string(str));
	put_expr(str);
    }
}

PRIVATE void
PutInEnvironment(idp)
register struct idf *idp;
/* When an exported variable is assigned to, calling this function results
 * in its value being put in the environment, so that programs started up by
 * Amake may make use of them.
 */
{
    DBUG_PRINT("putenv", ("putenv(%s)", idp->id_text));
    BufferStart();
    BufferAddString(idp->id_text);
    BufferAdd('=');
    AddAmakeValueToBuf(idp->id_variable->var_expr,
		       idp->id_variable->var_separator);
    PutEnv(idp, BufferResult());
}

PUBLIC void
EnterVarDef(idp, val)
struct idf *idp;
struct expr *val;
/*
 * Create a new assignment job and append it to the the ReadyJobs list.
 */
{
    struct variable *var_def;
    register struct job *j;

    DBUG_EXECUTE("assign", {
	DBUG_WRITE(("define %s = ", idp->id_text)); DBUG_Expr(val); DBUG_NL();
    });
    var_def = idp->id_variable;
    if ((var_def->var_flags & V_CMDLINE) != 0) {
	DBUG_PRINT("assign", ("skip assignment to `%s'", idp->id_text));
	return; /* ignore assignments to cmdline variables */
    }
    if (var_def->var_state == V_ALLOC) {
	var_def->var_state = V_INIT;
    } else {
	if (var_def->var_state != V_DONE) {
	    /* in the assignment the old value might be used */
	    StartExecution((struct job *)NULL);
	}
	assert(var_def->var_state == V_DONE); /* must be available now */
	var_def->var_state = V_AGAIN;
	var_def->var_old_value = var_def->var_expr;
    }
    var_def->var_expr = val;

    if ((idp->id_flags & F_NO_EVAL) == 0) {	
	if (ReadingCache) {
	    /* avoid job overhead while parsing Statefile */
	    if (idp == Id_cache_entry) {
		EnterStateEntry(val);
		var_def->var_state = V_DONE;
	    } else {
		extern int CacheScopeNr;

		/* attributes to be attached globally */
		CacheScopeNr = GLOBAL_SCOPE;
		DoAssignment(&var_def);
	    }
	} else {
	    j = CreateJob(ClassAssignment, (generic)var_def);
	    var_def->var_eval = j;
	    PutInReadyQ(j);
	}
    } else { /* don't evaluate internal identifiers, such as CACHE */
	var_def->var_state = V_DONE;
    }
}
    
PUBLIC struct expr *
GetVarValue(idp)
struct idf *idp;
/*
 * Return the value of the variable idp, or suspend the current job.
 */
{
    extern struct job *CurrentJob;
    register struct expr *res;

    DBUG_ENTER("GetVarValue");
    switch (idp->id_variable->var_state) {
    case V_DONE:    /* normal case: defined variable */
	res = CopyExpr(idp->id_variable->var_expr);
	break;
    case V_AGAIN:
	res = CopyExpr(idp->id_variable->var_old_value);
	break;
    case V_BUSY:
	assert(CurrentJob != NULL);
	/* watch out for "a = $a + $b", when a is not yet assigned */
	if (CurrentJob->job_indicator == ClassAssignment &&
	    ((struct variable *)JobInfo(CurrentJob)) == idp->id_variable) {
	    Warning("recursive assignment, `%s' defaults to ''",
		    idp->id_text);
	    res = StringExpr("");
	} else {
	    /* let the job wait till the value is known */
	    DBUG_PRINT("suspend", ("value of `%s'", idp->id_text));
	    SuspendCurrent(idp->id_variable->var_eval);
	    ReSched();
	    /*NOTREACHED*/
	}
	break;
    case V_ALLOC:	/* non imported environment variable */
	Warning("variable `%s' not imported, defaults to ''", idp->id_text);
	res = StringExpr("");
	break;
    default:
	CaseError("GetVarValue: %d", (int)idp->id_variable->var_state);
	/*NOTREACHED*/
    }
    DBUG_RETURN(res);
}

PUBLIC int
IsDefined(idp)
struct idf *idp;
{
    /* Is variable `idp' defined? Watch out for unimported environment vars*/
    return(idp->id_variable != NULL &&
	   idp->id_variable->var_state != V_ALLOC);
}

PUBLIC void
DoAssignment(var_defp)
struct variable **var_defp;
{
    register struct variable *var = *var_defp;

    DBUG_EXECUTE("resume", {
	if (var->var_state == V_BUSY) {
	    DBUG_PRINT("job", ("continue assignment to %s",
			       var->var_idp->id_text));
	}
    });

    if (var->var_state == V_AGAIN) {
	var->var_expr = Eval(var->var_expr);
	put_expr(var->var_old_value);
	var->var_old_value = NULL;
    } else {
	var->var_state = V_BUSY;
	var->var_expr = Eval(var->var_expr);
    }

    DBUG_PRINT("assign", ("%s->var_flags = %x", var->var_idp->id_text,
			  var->var_flags));

    if ((var->var_flags & V_FROM_ENV) != 0) {
	/* value came from environment, so don't put it in now. */
	var->var_flags &= ~V_FROM_ENV; /* but do put later assignments */
    } else {
	if ((var->var_flags & V_EXPORT) != 0) {
	    /* only put explicitly exported variables */
	    PutInEnvironment(var->var_idp);
	}
    }

    var->var_state = V_DONE;

    if (var->var_flags & V_BUILTIN) {
	BuiltinVarUpdate(var);
    }

    if ((var->var_expr->e_type & T_LIST_OF) != 0) {
	/* Share list values of variables as much as possible.
	 * This may reduce memory usage quite a lot, when things like
	 * 	CFLAGS = { -Ifirst_path, .., -Itenth_path};
	 * are used in cc-c invocations.
	 */
	MakeShared(var->var_expr);
    }

    DBUG_EXECUTE("assign", {
	DBUG_WRITE(("Assign %s := ", var->var_idp->id_text));
	DBUG_Expr(var->var_expr); DBUG_NL();
    });
}

PUBLIC void
DoImport(idp, sep)
struct idf *idp;
char *sep;
{
    char *env_value;

    if (idp->id_variable->var_separator != NULL) {
	if (strcmp(idp->id_variable->var_separator, sep) != 0) {
	    Warning("re-import of `%s' with other separator", idp->id_text);
	}
    }
    idp->id_variable->var_separator = NULL;
    if ((env_value = getenv(idp->id_text)) != NULL) {
	EnterVarDef(idp, EnvToAmake(env_value, sep));
    } else {
	Verbose(1, "`%s' not found in enviroment, defaults to ''",
		idp->id_text);
	EnterVarDef(idp, StringExpr(""));
    }
    idp->id_variable->var_flags |= V_IMPORT;
}

PUBLIC void
DoExport(idp, sep)
struct idf *idp;
char *sep;
{
    register struct variable *var = idp->id_variable;

    if (var->var_separator != NULL) {
	if (strcmp(sep, var->var_separator) != 0) {
	    Warning("re-export of `%s' with other separator", idp->id_text);
	}
    }
    var->var_separator = sep;
    var->var_flags |= V_EXPORT;

    if (var->var_state == V_DONE) {
	/* assignment already done: export it now */
	PutInEnvironment(var->var_idp);
    }
}

PRIVATE char *
PrAssignment(var_def)
struct variable *var_def;
{
    (void)sprintf(ContextString, "assignment to `%s'",
		 var_def->var_idp->id_text);
    return(ContextString);
}

/*ARGSUSED*/
PRIVATE void
RmAssignment(var_def)
struct variable *var_def;
{
}

PUBLIC void
InitAssignments()
{
    ClassAssignment = NewJobClass("assignment",
			  (void_func)DoAssignment, (void_func)RmAssignment, 
			  (string_func)PrAssignment);
    Environment = (char **)Malloc((unsigned) (ENV_INIT * sizeof(char*)));
    Environment[EnvSize = 0] = NULL;
    EnvLeft = ENV_INIT - 1;

    Id_dummy_var = str2idf("_dummy", 0);
    DeclareIdent(&Id_dummy_var, I_VARIABLE);
}
