/*	@(#)builtin.c	1.4	94/04/07 14:46:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "global.h"
#include "standlib.h"
#include "alloc.h"
#include "idf.h"
#include "expr.h"
#include "type.h"
#include "slist.h"
#include "scope.h"
#include "Lpars.h"
#include "dbug.h"
#include "objects.h"
#include "eval.h"
#include "execute.h"
#include "error.h"
#include "conversion.h"
#include "assignment.h"
#include "cluster.h"
#include "main.h"
#include "buffer.h"
#include "invoke.h"
#include "lexan.h"
#include "os.h"
#include "builtin.h"

PUBLIC  struct idf *Id_ROOT, *Id_AMAKELIB, *Id_PWD, *Id_if;

#define MAX_COMP_SIZE 256
#define EOS '\0'

/*
 * Function initialisation routines
 */

PRIVATE struct expr *NoDefault = NULL;

PRIVATE struct function *CurFun;

PRIVATE void Function	P((char *name, int type));
PRIVATE void Parameter	P((char *name, struct type *type, struct expr *def));
PRIVATE void Body	P((builtin_func fun));

PRIVATE struct cons *get_cons	  P((char *name, struct cons **argp));
PRIVATE struct expr *get_argument P((char *name, int type, struct cons **argp));

PRIVATE void
Function(name, type)
char *name;
int type;
{
    struct idf *idp;

    NewScope();
    idp = str2idf(name, 0);
    DeclareIdent(&idp, I_FUNCTION);
    CurFun = idp->id_function;
    CurFun->fun_result = type;
}

PRIVATE void
Parameter(name, type, default_expr)
char *name;
struct type *type;
struct expr *default_expr;
{
    struct idf *idp;

    idp = str2idf(name, 0);
    DeclareIdent(&idp, I_PARAM);
    idp->id_param->par_type = type;
    idp->id_param->par_default = default_expr;
}

PRIVATE void
Body(fun)
builtin_func fun;
{
    CurFun->fun_scope = CurrentScope;
    CurFun->fun_code = fun;
    ClearScope();
}

PRIVATE struct cons *
get_cons(name, argp)
char *name;
struct cons **argp;
/* Must be called in the same order the parameters were defined.
 * Efficiency reasons, of course.
 */
{
    register struct cons *cons = *argp;

#ifdef LINT
    name = name;
#endif
    assert(cons != NULL);
    assert(!strcmp(name, ItemP(cons,expr)->e_left->e_param->par_idp->id_text));
    *argp = Next(*argp);
    return(cons);
}

PRIVATE struct expr *
get_argument(name, type, argp)
char *name;
int type;
struct cons **argp;
/* Must be called in the same order the parameters were defined.
 * Efficiency reasons, of course.
 */
{
    register struct expr *arrow;

    arrow = ItemP(get_cons(name, argp), expr);
    arrow->e_right = GetExprOfType(arrow->e_right, type);
    return(arrow->e_right);
}

#ifdef DEBUG
PRIVATE void
end_args(argp)
struct cons **argp;
{
    assert(*argp == NULL);	/* all arguments eaten */
}
#else
#define end_args(argp)		{ /* do nothing */ }
#endif

#define LCOMP(obj) ((obj)->e_object->ob_idp->id_text)

PRIVATE struct expr *
lcomp_fn(arg_list)
struct slist *arg_list;
{
    struct expr *obj, *res;
    struct cons *curr = HeadOf(arg_list);

    DBUG_ENTER("lcomp_fn");
    obj = get_argument("object", T_OBJECT, &curr);
    end_args(&curr);
    res = StringExpr(LCOMP(obj));
    DBUG_RETURN(res);
}

PUBLIC struct expr *
DoGetAttrValues(objp, attr)
struct expr **objp;
struct idf *attr;
{
    register struct expr *res;

    if (((*objp)->e_type & T_LIST_OF) == 0) {
	/* simple case: don't create list when it's not necessary */
	*objp = GetObject(*objp);
	res = CopyExpr(GetAttrValue((*objp)->e_object, attr));
    } else {
	/* get attributes of a list of objects */
	*objp = GetExprOfType(*objp, T_LIST_OF | T_OBJECT);

	/* first derive all attributes (see the comment at select_fn) */
	ITERATE((*objp)->e_list, expr, obj, {
	    (void) GetAttrValue(obj->e_object, attr);
	});

	/* we made it */
	res = empty_list();
	ITERATE((*objp)->e_list, expr, obj, {
	    Append(&res->e_list, CopyExpr(GetAttrValue(obj->e_object, attr)));
	});
	res = Eval(res); /* no lists within the resultlist */
    }

    return(res);
}

PRIVATE struct expr *
get_fn(arg_list)
struct slist *arg_list;
/* get attribute value of object or list of objects */
{
    struct idf *idp;
    struct expr *arrow, *attr;
    struct cons *curr = HeadOf(arg_list);

    DBUG_ENTER("get_fn");
    arrow = ItemP(get_cons("objects", &curr), expr);
    attr = get_argument("attribute", T_STRING, &curr);
    end_args(&curr);

    idp = get_id(attr);
    DeclareAttribute(idp);	/* maybe a completely new one */

    DBUG_RETURN(DoGetAttrValues(&arrow->e_right, idp));
}

PRIVATE struct expr *
echo_fn(arg_list)
struct slist *arg_list;
/* echo the strings, given as argument */
{
    struct expr *strings;
    struct cons *curr = HeadOf(arg_list);

    DBUG_ENTER("echo_fn");
    strings = get_argument("strings", T_LIST_OF | T_STRING, &curr);
    end_args(&curr);

    ITERATE(strings->e_list, expr, str, {
	Message("%s ", do_get_string(str));
    });
    Message("\n");
    DBUG_RETURN(TrueExpr);
}

PRIVATE struct expr *
exit_fn(arg_list)
struct slist *arg_list;
/* echo the strings, given as argument, and then exit */
{
    extern void Exit();
    struct expr *strings;
    struct cons *curr = HeadOf(arg_list);

    DBUG_ENTER("exit_fn");
    strings = get_argument("strings", T_LIST_OF | T_STRING, &curr);
    end_args(&curr);

    if (!IsEmpty(strings->e_list)) {
	ITERATE(strings->e_list, expr, str, {
	    Message("%s ", do_get_string(str));
	});
	Message("\n");
    }
    Exit();
    /*NOTREACHED*/
}

PRIVATE struct expr *
select_fn(arg_list)
struct slist *arg_list;
/*
 * select(objlist, attr, val)
 *
 * return exactly those objects from `objlist' that have value `val' for
 * attribute `attr'
 */
{
    struct expr *objects, *attr, *value;
    struct expr *result;
    struct cons *curr = HeadOf(arg_list);
    struct idf *idp;

    DBUG_ENTER("select_fn");
    objects = get_argument("objects", T_LIST_OF | T_OBJECT, &curr);
    attr = get_argument("attr", T_STRING, &curr);
    value = ItemP(get_cons("value", &curr), expr)->e_right;
    end_args(&curr);

    idp = get_id(attr);
    DeclareAttribute(idp);	/* maybe a completely new one */

    /*
     * For safety, we GetAttr() all the attributes first, before doing
     * the select. Otherwise, if `attr' requires a derivation to be called,
     * which then calls an exec(), our results accumulated would be lost and
     * never be freed.
     */
    ITERATE(objects->e_list, expr, obj, {
	(void) GetAttrValue(obj->e_object, idp);
    });

    /* we made it */
    result = empty_list();
    ITERATE(objects->e_list, expr, obj, {
	register struct expr *attrval = GetAttrValue(obj->e_object, idp);

	if (is_equal(attrval, &value)) {
	    Append(&result->e_list, obj);
	}
    });

    /* set the type */
    if (!IsEmpty(result->e_list)) {
	result->e_type =
	    T_LIST_OF | ItemP(HeadOf(result->e_list), expr)->e_type;
    } else {
	result->e_type = T_LIST_OF | T_ANY;
    }

    DBUG_RETURN(result);
}

PUBLIC void
get_exec_args(arg_list, prog, args, standin, standout, standerr)
struct slist *arg_list;
struct expr **prog, **args, **standin, **standout, **standerr;
{
    register struct expr *arrow;
    struct cons *curr = HeadOf(arg_list);

    *prog = get_argument("prog", T_OBJECT, &curr);
    arrow = ItemP(get_cons("args", &curr), expr);
    *args = arrow->e_right = GetList(arrow->e_right);
    *standin = get_argument("stdin", T_OBJECT, &curr);

    /* Get the standard output argument. It may still have to be evaluated */
    arrow = ItemP(get_cons("stdout", &curr), expr);
    if (arrow->e_right->e_number == DIAG) {
	put_expr_node(arrow->e_right);
	arrow->e_right = ObjectExpr(NewDiagFile());
    } else {
	arrow->e_right = GetObject(arrow->e_right);
    }
    *standout = arrow->e_right;

    /* same for standard error */
    arrow = ItemP(get_cons("stderr", &curr), expr);
    if (arrow->e_right->e_number == DIAG) {
	put_expr_node(arrow->e_right);
	arrow->e_right = ObjectExpr(NewDiagFile());
    } else {
	arrow->e_right = GetObject(arrow->e_right);
    }
    *standerr = arrow->e_right;

    end_args(&curr);
}

#ifdef AMOEBA

/* On Amoeba we don't known whether the arguments should be escaped, because
 * at this point we don't know wether a command should be executed with
 * /bin/sh or not.
 */
#define EscapedBufferAddString(s) BufferAddString(s)

#else

PRIVATE void
EscapedBufferAddString(str)
char *str;
{
    register char *p;

    for (p = str; *p != 0; p++) {
	if (strchr(SHELL_METAS, *p) != 0) { /* must be escaped */
	    BufferAdd('\\');
	}
	BufferAdd(*p);
    }
}

#endif

PUBLIC char *
CmdObjectName(obj)
struct object *obj;
{
    /* Trust that object arguments don't contain meta characters
     * A correct and efficient way to do it is to set a special
     * flag when the object is created. That flag can be checked 
     * here.
     */
    if (F_link_opt &&
	(obj->ob_flags & O_OUTPUT) == 0 && obj->ob_calias != NULL) {
	/* If the O.S. supports big command lines (more than, say, 10K bytes)
	 * we try to avoid links & unlinks as much as possible.
	 * The command then has to use the cached object as input.
	 * For the recreation of a big library (e.g. libamoeba.a) this
	 * trick might save 100's of link() & unlink() calls.
	 */
	return(SystemName(obj->ob_calias));
    } else {
	return(SystemName(obj));
    }
}

PUBLIC char *
command_string(arg_list, redirect)
struct slist *arg_list;
char **redirect;
{
    struct expr *prog, *args, *standin, *standout, *standerr;
    char redir[1000];

    DBUG_ENTER("command_string");
    get_exec_args(arg_list, &prog, &args, &standin, &standout, &standerr);

    /* Currently we just make a command string suitable for letting /bin/sh
     * do the real command execution.
     * Faster methods certainly exist.  Nmake, for example, uses a sort of
     * "co-process" that takes care of command execution.
     */
    BufferStart();
    BufferAddString(SystemName(prog->e_object));

    ITERATE(args->e_list, expr, arg, {
	BufferAdd(' ');
	if (arg->e_type == T_OBJECT) {
	    BufferAddString(CmdObjectName(arg->e_object));
	} else {
	    Item(cons) = (generic)GetExprOfType(arg, T_STRING);
	    EscapedBufferAddString(do_get_string(ItemP(cons, expr)));
	}
    });

    redir[0] = '\0'; /* initially empty */

    if (standin->e_object != FooObject) {
	/* By default don't redirect standard input, because some
	 *(non-portable) constructions, like pipes in the 'args' parameter
	 * won't work.
	 */
	strcat(redir, " <"); strcat(redir, CmdObjectName(standin->e_object));
    }
	
    strcat(redir, " >"); strcat(redir, SystemName(standout->e_object));
    if (standout->e_object == standerr->e_object) {
	/* don't let standard out and standard error overwrite each other */
	strcat(redir, " 2>&1");
    } else {
	strcat(redir, " 2>"); strcat(redir, CmdObjectName(standerr->e_object));
    }

    *redirect = Salloc(redir, (unsigned)(strlen(redir) + 1));
    DBUG_RETURN(BufferResult());
}

#ifdef AtariSt
#define MAX_SIZE	30000
#else
#define MAX_SIZE	1000000	/* should be enough */
#endif

PRIVATE void
AppendString(listp, text, size)
struct slist **listp;
char *text;
int size;
{
    register struct expr *str;

    str = new_expr(STRING);
    str->e_type = T_STRING;	/* a byte sequence to be precise */
    str->e_string = Salloc(text, (unsigned)(size + 1));
    *(str->e_string + size) = EOS;
    Append(listp, str);
}

PRIVATE struct expr *
MakeStringList(text, size)
char *text;
int size;
{
    register char *ptr, *head;
    struct expr *list;

    list = empty_list();
    list->e_type = T_LIST_OF | T_STRING;

    head = text;
    while ((ptr = strchr(head, '\n')) != NULL) {
	AppendString(&list->e_list, head, (int)(ptr - head));
	head = ptr + 1;
    }
    if (head != text + size) {
	DBUG_PRINT("return", ("output not terminated with newline"));
	/* text *was* terminated with EOS */
	AppendString(&list->e_list, head, (int)((text + size) - head));
    }
    free(text);
    return(list);
}

PRIVATE int
ReadFile(filename, contents)
char *filename;
char **contents;
{
    char *text = NULL;
    long size;

    if ((size = GetFileSize(filename)) < 0) {
	/* command probably couldn't be exec()ed due to lack of memory */
	Verbose(2, "couldn't get size of %s", filename);
    } else if (size != 0) {
	FILE *fp;

	if (size > MAX_SIZE) {
	    Warning("size of `%s' (%ld) exceeds %d", size, MAX_SIZE);
	    size = MAX_SIZE;
	}
	if ((fp = fopen(filename, "r")) == NULL) {
	    Warning("couldn't open `%s'", filename);
	} else {
	    int n;

	    DBUG_PRINT("read", ("read `%s'", filename));
	    text = Malloc((unsigned int)size + 1);

	    if ((n = fread(text, 1, (int) size, fp)) < 0) {
		Warning("couldn't read `%s'", filename);
		text = Salloc("", (unsigned)1);
		size = 0;
	    } else if (n != size) {
		Warning("`%s': read %d instead of expected %ld characters",
			filename, n, size);
		text = Realloc(text, (unsigned)(n + 1));
		size = n;
	    }
	    text[size] = EOS;
	    fclose(fp);
	}
    } else {
	DBUG_PRINT("read", ("`%s' is empty", filename));
    }

    if (text == NULL) {
	*contents = Salloc("", (unsigned)1);
	return(0);
    } else {
	*contents = text;
	return(size);
    }
}

PRIVATE struct expr *
Contents(obj)
struct object *obj;
{
    int size;
    struct expr *res;
    char *text;

    if (F_no_exec && ((obj->ob_flags & O_DIAG) != 0 ||
		      IsTrue(GetAttrValue(obj, Id_changed)))) {
	text = Salloc("", (unsigned)1); size = 0;
    } else {
	if ((res = GetAttrValue(obj, Id_contents)) != UnKnown) {
	    DBUG_PRINT("read", ("cached contents of `%s'", SystemName(obj)));
	    return(CopyExpr(res));	/* cached contents */
	}
	size = ReadFile(SystemName(obj), &text);
    }
    res = MakeStringList(text, size);
    if (obj->ob_parent == AmakeDir) {
	/* object resides in cache, contents of others are not cached */
	PutAttrValue(obj, Id_contents, CopyExpr(res), GLOBAL_SCOPE);
    }
    return(res);
}

PUBLIC struct expr *
GetContents(obj, follow, remove)
struct object *obj;
int follow, remove;
{
    struct expr *alias, *res;
    struct object *the_obj;

    PrintObject(obj);
    if (follow && ((alias = GetAttrValue(obj, Id_alias)) != UnKnown)) {
	assert(alias->e_number == OBJECT);
	the_obj = alias->e_object;
	remove = FALSE; /* don't remove objects in cache */
    } else {
	the_obj = obj;
    }
    res = Contents(the_obj);
    if (remove & !F_no_exec) {
	DoDelete(SystemName(the_obj), NON_FATAL);
    }
    return(res);
}

PRIVATE void
DoResult(file, res)
struct expr *file;
struct expr **res;
{
    int do_remove;
    struct object *obj = file->e_object;

    assert(file->e_number == OBJECT);

    if ((obj->ob_flags & O_RETURN) != 0) {
	struct expr *alias;
	struct object *the_obj;

	if ((alias = GetAttrValue(obj, Id_alias)) != UnKnown) {
	    assert(alias->e_number == OBJECT);
	    the_obj = alias->e_object;
	    do_remove = FALSE; /* leave objects in cache alone */
	} else {
	    the_obj = obj;
	    do_remove = TRUE;
	}

	if ((obj->ob_flags & O_RETURN) != 0) {
	    if (ReqType == (T_LIST_OF | T_STRING)) {
		assert(*res == NULL);	/* only one RETURN allowed */
		PrintObject(the_obj);
		if (IsTrue(GetAttrValue(obj, Id_failed))) {
		    (*res = empty_list())->e_type = T_LIST_OF | T_STRING;
		} else {
		    *res = Contents(the_obj);
		}
	    }
	    return; /* at this moment, don't remove tmp file (when?) */
	}
	if (do_remove && !F_no_exec) {
	    DoDelete(SystemName(the_obj), NON_FATAL);
	}
    }
}

PUBLIC struct expr *
ExecResult(arg_list, status)
struct slist *arg_list;
struct expr *status;
/* Deliver the result of an exec-call, which depends on exit status
 * and presence of a %return directive.
 */
{
    struct expr *prog, *args, *standin, *standout, *standerr;
    struct expr *res = NULL;

    get_exec_args(arg_list, &prog, &args, &standin, &standout, &standerr);

    DoResult(standout, &res);
    DoResult(standerr, &res);

    if (res == NULL) { /* no RETURN directive: just deliver status */
	res = status;
    }
    return(res);
}

PRIVATE int DiagnosticsWritten;

PRIVATE void
ShowDiag(text, vproc, exec)
char *text;
char *vproc;
struct job *exec;
{
    if (!DiagnosticsWritten) {
	Message("# [%s] diagnostics for %s\n", vproc, ExecContext(exec));
	DiagnosticsWritten = TRUE;
    }
    Message("# ");
    SafeMessage(text);
    Message("\n");
}

PRIVATE void
DoDiag(obj, vproc, exec)
struct object *obj;
char *vproc;
struct job *exec;
{
    if ((obj->ob_flags & O_DIAG) != 0) { /* only when it's a diagnostic file */
	char *str, *ptr, *text;

	if (!F_no_exec) {
	    int size = ReadFile(SystemName(obj), &text);

	    str = text;
	    while ((ptr = strchr(str, '\n')) != NULL) {
		*ptr = '\0';
		ShowDiag(str, vproc, exec);
		str = ptr + 1;
	    }
	    if (str != text + size) {
		/* output no terminated with newline, but text *was*
		 * terminated with EOS (by ReadFile), so:
		 */
		ShowDiag(str, vproc, exec);
	    }
	    free(text);
	}
	PutDiagFile(obj);
    }
}
	
PUBLIC void
HandleDiagnostics(arg_list, vproc, exec)
struct slist *arg_list;
char *vproc;
struct job *exec;
/* show diagnostics, if available. */
{
    struct expr *prog, *args, *standin, *standout, *standerr;

    get_exec_args(arg_list, &prog, &args, &standin, &standout, &standerr);
    DiagnosticsWritten = FALSE;
    DoDiag(standout->e_object, vproc, exec);
    if (standerr->e_object != standout->e_object) {
	/* don't show the same contents twice if redirected to the same file */
	DoDiag(standerr->e_object, vproc, exec);
    }
}

#define MATCH_CHAR	'%'

PRIVATE struct expr *
matches_fn(arg_list)
struct slist *arg_list;
{
    int blen, plen;
    struct cons *curr = HeadOf(arg_list);
    char *lcomp, *pattern;
    register char *b, *p;
    struct expr *object, *res;

    DBUG_ENTER("matches_fn");
    object = get_argument("object", T_OBJECT, &curr);
    pattern = do_get_string(get_argument("pattern", T_STRING, &curr));
    end_args(&curr);

    lcomp = LCOMP(object);
    res = FalseExpr; /* I am known for my positive thinking */

    /* skip the parts that match exactly */
    for (b = lcomp, p = pattern; *b == *p ; b++, p++) {
	if (*p == EOS || *p == MATCH_CHAR)
	    break;
    }
    if (*p == MATCH_CHAR) {
	/* the part of pattern after MATCH_CHAR must match
	   the last characters of lcomp exactly
	   */
	p++;
	blen = strlen(b);
	plen = strlen(p);
	if (blen > plen && strcmp(p, b + blen - plen) == 0) {
	    res = TrueExpr;
	}
    }
    DBUG_RETURN(res);
}

PRIVATE struct expr *
basename_fn(arg_list)
struct slist *arg_list;
{
    int base_length;
    struct cons *curr = HeadOf(arg_list);
    struct expr *object;
    char *lcomp, *pattern;
    register char *b, *p;

    DBUG_ENTER("basename_fn");
    object = get_argument("object", T_OBJECT, &curr);
    pattern = do_get_string(get_argument("pattern", T_STRING, &curr));
    end_args(&curr);

    lcomp = LCOMP(object);

    /* skip the parts that match exactly */
    for (b = lcomp, p = pattern; *b == *p ; b++, p++) {
	if (*p == EOS || *p == MATCH_CHAR)
	    break;
    }
    if (*p == MATCH_CHAR) {
	/* the part of pattern after MATCH_CHAR must match
	   the last characters of lcomp exactly
	   */
	p++;
	base_length = strlen(b) - strlen(p);
	if (base_length > 0 && strcmp(p, b + base_length) == 0) {
	    char base[MAX_COMP_SIZE];

	    strncpy(base, b, base_length); /* copy twice? */
	    base[base_length] = '\0';
	    DBUG_RETURN(StringExpr(base));
	}
    }
    /* base couldn't be found */
    DBUG_RETURN(StringExpr(SystemName(object->e_object)));
}

PRIVATE char *
FillPat(pattern, base)
char *pattern, *base;
{
    static char filled[MAX_COMP_SIZE];
    char *p = pattern, *f = filled;

    while (*p != EOS && *p != MATCH_CHAR) {
	*f++ = *p++;
    }
    *f = EOS;
    if (*p == MATCH_CHAR) {
	strcat(filled, base);
	strcat(filled, p+1);
    } else {
	FatalError("pattern without `%%'");
    }
    return(filled);
}


/*
 * ToDo: Introduce special attributes like `dir', `name' and `base' that have
 * a predefined interpretation / derivation.
 * The `base' attribute may standard be derived by the (hypothetical) rule
 * %derive f [base = part-before-dot($f)] %when has-one-dot($f)
 */

extern struct param *CurrentParameter; /* in cluster.c */

PRIVATE struct expr *
match_fn(arg_list)
struct slist *arg_list;
{
    struct cons *curr = HeadOf(arg_list);
    struct expr *res = empty_list();
    struct expr *objects, *pattern, *basename;
    struct slist *attributes;
    struct object *new_obj;
    register struct cons *cons;

    DBUG_ENTER("match_fn");
    objects = get_argument("objects", T_LIST_OF | T_OBJECT, &curr);
    end_args(&curr);

    attributes = (CurrentParameter->par_type->tp_attributes != NULL) ?
 	          CurrentParameter->par_type->tp_attributes->e_list : NULL;
    RemoveAttributes(Anonymous); /* remove all previous attributes */
    AddAttrList(ObjectExpr(Anonymous), attributes);
    if ((pattern = GetAttrValue(Anonymous, Id_defpat)) == UnKnown) {
	FatalError("couldn't derive attribute `%s' of parameter `%s'",
		   Id_defpat->id_text, CurrentParameter->par_idp->id_text);
    }
    /* problem: if this causes an exec, we are in trouble (where have we
     * heard this before). For a complete solution: look at TryDerivation
     * in derive.c, and factor out the corresponding parts.
     */
    for (cons = Head(objects->e_list); cons != NULL; cons = Next(cons)) {
	struct object *src = ItemP(cons, expr)->e_object;
	struct idf *newid;

	if ((basename = GetAttrValue(src, Id_base)) != UnKnown) {
	    newid = str2idf(FillPat(do_get_string(pattern),
				    do_get_string(basename)), 1);
	    new_obj = ObjectWithinParent(newid, Context);
	    Append(&res->e_list, ObjectExpr(new_obj));
	} else {
	    PrintObject(Anonymous);
	    PrintObject(src);
	    FatalError("couldn't derive attribute `base' of object `%s'",
		       ObjectName(src));
	}
    }
    res->e_type = T_LIST_OF | T_OBJECT;
    DBUG_RETURN(res);
}


PRIVATE struct expr *
vpath_fn(arg_list)
struct slist *arg_list;
{
    struct cons *curr = HeadOf(arg_list);
    struct expr *res, *name, *dirs;
    struct object *try;
    struct idf *obj_id;

    DBUG_ENTER("vpath_fn");
    name = get_argument("name", T_STRING, &curr);
    dirs = get_argument("dirs", T_LIST_OF|T_OBJECT, &curr);
    end_args(&curr);

    obj_id = str2idf(do_get_string(name), 1);
    res = ObjectExpr(Context);	/* default if not found */
    ITERATE(dirs->e_list, expr, dir, {
	try = ObjectWithinParent(obj_id, dir->e_object);
	if (FileExists(try)) {
	    res = ObjectExpr(dir->e_object); /* note this has changed ! */
	    break;
	}
    });
    DBUG_RETURN(res);
}

PRIVATE struct expr *
move_fn(arg_list)
struct slist *arg_list;
{
    char from_name[256]; 
    struct cons *curr = HeadOf(arg_list);
    struct expr *from, *to;

    DBUG_ENTER("move_fn");
    from = get_argument("from", T_OBJECT, &curr);
    to   = get_argument("to",   T_OBJECT, &curr);
    end_args(&curr);

    (void) strcpy(from_name, SystemName(from->e_object));
    DBUG_RETURN(F_no_exec || (rename(from_name, SystemName(to->e_object)) == 0)
		? TrueExpr : FalseExpr);
}

PRIVATE struct expr *
defined_fn(arg_list)
struct slist *arg_list;
{
    struct cons *curr = HeadOf(arg_list);
    struct expr *var;

    DBUG_ENTER("defined_fn");
    var = get_argument("var", T_STRING, &curr);
    end_args(&curr);
    DBUG_RETURN(IsDefined(str2idf(do_get_string(var), 1)) ? TrueExpr : FalseExpr);
}

PRIVATE struct expr *
strip_fn(arg_list)
struct slist *arg_list;
/* strip tries to strip the head off a string, i.e.,
 * strip(aapnoot, aap)  ::= noot;
 * strip(aapnoot, mies) ::= aapnoot;
 */
{
    struct cons *curr = HeadOf(arg_list);
    struct expr *from, *strip, *result;
    int len;

    from = get_argument("string1", T_STRING, &curr);
    strip = get_argument("string2", T_STRING, &curr);
    end_args(&curr);
    len = strlen(do_get_string(strip));

    if (strncmp(do_get_string(from), do_get_string(strip), len) == 0) {
	result = StringExpr(do_get_string(from) + len);
    } else {
	result = CopyExpr(from);
    }
    return(result);
}

PRIVATE struct expr *
parent_fn(arg_list)
struct slist *arg_list;
{
    struct cons *curr = HeadOf(arg_list);
    struct expr *obj;

    obj = get_argument("obj", T_OBJECT, &curr);
    end_args(&curr);
    return(ObjectExpr(obj->e_object->ob_parent));
}

#define ARG(cons) (ItemP(cons, expr)->e_right)

PRIVATE struct expr *
if_fn(arg_list)
struct slist *arg_list;
{
    struct cons *cond_cons, *then_cons, *else_cons;
    struct expr *res;
    struct cons *curr = HeadOf(arg_list);

    DBUG_ENTER("if_fn");
    cond_cons = get_cons("cond", &curr);
    then_cons = get_cons("then", &curr);
    else_cons = get_cons("else", &curr);
    end_args(&curr);
    if (IsTrue(ARG(cond_cons) = GetBoolean(Eval(ARG(cond_cons))))) {
	res = ARG(then_cons) = Eval(ARG(then_cons));
    } else {
	res = ARG(else_cons) = Eval(ARG(else_cons));
    }
    DBUG_RETURN(CopyExpr(res));
}

PUBLIC void
BuiltinVarUpdate(var)
struct variable *var;
{
    /* currently only AMAKELIB is handled in a special way */
    if (var->var_idp == Id_AMAKELIB) {
	var->var_expr = GetExprOfType(var->var_expr, T_LIST_OF | T_OBJECT);
	SetDirTab(var->var_expr->e_list);
    }
}

PUBLIC void
builtin_init(lib_dirs)
struct slist *lib_dirs;
{
    struct expr *expr;
    struct type *ObjectListType;

    ObjectListType = new_type();
    ObjectListType->tp_indicator = T_LIST_OF | T_OBJECT;
    ObjectListType->tp_attributes = empty_list();

    /* initialise variable ROOT */
    Id_ROOT = str2idf("ROOT", 0);
    DeclareIdent(&Id_ROOT, I_VARIABLE);
    Id_ROOT->id_variable->var_flags |= V_BUILTIN;
    EnterVarDef(Id_ROOT, ObjectExpr(RootDir));

    /* variable PWD, which contains the parent of the file being read from
     * will be assigned to in the lexical analyser.
     */
    Id_PWD = str2idf("PWD", 0);
    DeclareIdent(&Id_PWD, I_VARIABLE);
    Id_PWD->id_variable->var_flags |= V_BUILTIN | V_INLINE;
    EnterVarDef(Id_PWD, ObjectExpr(Context)); /* not always correct */

    /* initialise variable AMAKELIB */
    Id_AMAKELIB = str2idf("AMAKELIB", 0);
    DeclareIdent(&Id_AMAKELIB, I_VARIABLE);
    (expr = empty_list())->e_type = T_LIST_OF | T_OBJECT;
    Id_AMAKELIB->id_variable->var_flags |= V_BUILTIN;
    expr->e_list = lib_dirs;
    EnterVarDef(Id_AMAKELIB, expr);

    /* initialise variable _if, used in handling conditionals */
    Id_if = str2idf("_if", 0);
    DeclareIdent(&Id_if, I_VARIABLE);
    Id_if->id_variable->var_flags |= V_BUILTIN;

    /* $ROOT probably is referrred to in the cache. */
    StartExecution((struct job *)NULL); /* initialise ROOT and AMAKELIB */

    /* exec spawns a command */
    Function("exec", T_BOOLEAN);
    Parameter("prog", ObjectType, NoDefault);
    Parameter("args", ListStringType, EmptyListStringExpr);
    Parameter("stdin", ObjectType, ObjectExpr(FooObject));
    /* command_string() can now see if input redirection is necessary */
    Parameter("stdout", ObjectType, DiagExpr);
    Parameter("stderr", ObjectType, DiagExpr);
    Body(exec_fn);

    /* defined(variable) delivers TRUE iff `variable' has been assigned to */
    Function("defined", T_BOOLEAN);
    Parameter("var", StringType, NoDefault);
    Body(defined_fn);

    /* function 'matches' tells if a name matches a pattern */
    Function("matches", T_BOOLEAN);
    Parameter("object", ObjectType, NoDefault);
    Parameter("pattern", StringType, NoDefault);
    Body(matches_fn);

    /* function 'strip' tries to strip the head off a string */
    Function("strip", T_STRING);
    Parameter("string1", StringType, NoDefault);
    Parameter("string2", StringType, NoDefault);
    Body(strip_fn);

    /* function 'parent' returns the parent of an object */
    Function("parent", T_OBJECT);
    Parameter("obj", ObjectType, NoDefault);
    Body(parent_fn);

    /* unix-like 'basename' builtin */
    Function("basename", T_STRING);
    Parameter("object", ObjectType, NoDefault);
    Parameter("pattern", StringType, NoDefault);
    /* What about default `get(object, def-pat)' or is this not possible?
       In tools it *does* work.
       Other solution: Put

       DERIVE f [base = basename(f, get(f, def-pat))]
       WHEN get(f, def-pat) <> UNKNOWN

       in default included Amakefile.
     */
    Body(basename_fn);

    /* function 'echo' prints a list of strings on standard output */
    Function("echo", T_BOOLEAN);
    Parameter("strings", ListStringType, EmptyListStringExpr);
    Body(echo_fn);

    /* function 'exit' exits Amake after performing an echo */
    Function("exit", T_BOOLEAN);
    Parameter("strings", ListStringType, EmptyListStringExpr);
    Body(exit_fn);
    
    /* function 'select' returns objects having a specific <attr, val> pair */
    Function("select", T_ANY);
    Parameter("objects", ObjectListType, NoDefault);
    Parameter("attr", StringType, NoDefault);
    Parameter("value", AnyType, NoDefault);
    Body(select_fn);

    /* function 'get' gets the value of a certain attribute of an object */
    Function("get", T_ANY);
    Parameter("objects", ObjectListType, NoDefault);
    Parameter("attribute", StringType, NoDefault);
    Body(get_fn);

    /* match */
    Function("match", T_LIST_OF | T_OBJECT);
    Parameter("objects", ObjectListType, NoDefault);
    Body(match_fn);

    /* lcomp gives last component of the full pathname of an object */
    Function("lcomp", T_STRING);
    Parameter("object", ObjectType, NoDefault);
    Body(lcomp_fn);

    /* if is a special function */
    Function("if", T_ANY);
    CurFun->fun_idp->id_flags |= F_SPECIAL_FUNC;
    Parameter("cond", BooleanType, NoDefault);
    Parameter("then", AnyType, NoDefault);
    Parameter("else", AnyType, NoDefault);
    Body(if_fn);

    /* move renames files */
    Function("move", T_BOOLEAN);
    Parameter("from", ObjectType, NoDefault);
    Parameter("to", ObjectType, NoDefault);
    Body(move_fn);

    /* vpath: find object in list of directories, UnKnown if not found */
    Function("vpath", T_OBJECT);
    Parameter("name", StringType, NoDefault);
    Parameter("dirs", ObjectListType,  NoDefault);
    Body(vpath_fn);
}


