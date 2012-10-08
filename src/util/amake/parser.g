/*	@(#)parser.g	1.3	94/04/07 14:53:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* pseudo tokens (not delivered by lexan): */
%token OBJECT;	/* used in expressions to indicate an Amake Object */
%token FORMAL;	/* used in function call expression, links to PARAM struct */
%token PARAM;	/* used in expressions to indicate a parameter REFERENCE */
%token BUSY;	/* used in expressions being computed, causes suspension */
%token POINTER; /* used to use any pointer as attribute value */

/*
 * The token NO_TOK has to be defined lastly. It is used to tell how big the
 * arrays containing information about (a subset of) the tokens have to be.
 * Also, the fact that LLgen assigns numbers from 256 onward, is used there.
 */
%token NO_TOK;

%lexical lexan;			/* interface to the lexical analyser */
%start LLparse, amake_program;	/* parser routine and starting symbol */
%start LLstatefile, state_file; /* statefile format */
%start LLcmdline, cmd_line_def; /* to handle -Dvar=value or -Dvar */

{
#include <assert.h>
#include <stdio.h>
#include "global.h"
#include "alloc.h"
#include "dbug.h"
#include "idf.h"
#include "lexan.h"
#include "Lpars.h"
#include "error.h"
#include "expr.h"
#include "eval.h"
#include "scope.h"
#include "check.h"
#include "type.h"
#include "main.h"
#include "symbol2str.h"
#include "slist.h"
#include "objects.h"
#include "derive.h"
#include "execute.h"
#include "assignment.h"
#include "declare.h"
#include "cluster.h"
#include "tools.h"
#include "builtin.h"
#include "generic.h"
#include "conversion.h"
#include "statefile.h"
#include "parser.h"

}

{
PUBLIC int ParsingConstruction;
/* TRUE iff we are currently parsing an Amake construction */

PRIVATE int InlineExpansion = TRUE;
/* Inline expansion of known variables is temporarily turned off (on Greg
 * Sharps request) when a tool definition is read.
 */

PRIVATE struct expr *NewInstance = NULL;
PRIVATE char *ConditionalText = NULL;
}

/* 
 * An Amake program consists of an arbitrary sequence of tool definitions,
 * attribute declarations and derivations, and cluster definitions.
 */

amake_program:
	[ { ParsingConstruction = TRUE; }
	  [ tool_def
	  | declaration
	  | derivation
	  | inclusion
	  | cluster_def
	  | %prefer conditional
	  | variable_def
	  | generic_def
	  | instantiation
	  | default_def
	  | import
	  | export
	  ]
	  ';'
	  /* eat it before instantiation or conditional insertion */
	  { ParsingConstruction = FALSE;
	    if (NewInstance != NULL) {
	        MakeInstance(NewInstance); /* NewInstance is freed */
		NewInstance = NULL;
	    } else if (ConditionalText != NULL) {
		DoInsertText(ConditionalText);
		/* free(ConditionalText); */ /* not now! */
		ConditionalText = NULL;
	    }
	  }
	]*
	;

generic_def
	{ struct idf *idp;
	  struct Generic *gen;
	}:
	GENERIC
	ID
	{ idp = Tok.t_idp;
	  DeclareIdent(&idp, I_GENERIC);
	  gen = idp->id_generic;
	  NewScope();
	}
	'('
	[ ID 		{ DeclareIdent(&Tok.t_idp, I_PARAM); }
	  [ ',' ID	{ DeclareIdent(&Tok.t_idp, I_PARAM); } ]*
	| /* empty */
	]
	{ gen->gen_scope = CurrentScope;
	  PrintScope(CurrentScope);
	}
	')'
	'{'
	{ gen->gen_text = ReadBody(); }
	'}'
	{ ClearScope();	}
	;

instantiation
	{ struct expr *inst;
	  int bad = FALSE;
	}:
	INSTANCE
	ID
	{ if ((Tok.t_idp->id_kind & I_GENERIC) == 0) {
	      SemanticIdError(Tok.t_idp, "not a generic");
	      Tok.t_idp->id_kind |= I_ERROR;
	      bad = TRUE;
	  }
	  inst = MakeCallExpr(Tok.t_idp);
	}
	'(' arg_list(inst, TRUE) ')'
	{ if (!bad) NewInstance = inst;	}
	;
			
conditional
	{ extern struct idf *Id_if;
	  char *text;
	  struct expr *cond;
	}:
	IF '(' expression(MIN_PRIO, &cond)
	{ EnterVarDef(Id_if, cond);
	  StartExecution((struct job *)NULL);
	  cond = Id_if->id_variable->var_expr =
		GetExprOfType(Id_if->id_variable->var_expr, T_BOOLEAN);
	}
	',' '{'
	{ text = ReadBody();
	  if (IsTrue(cond)) {
	      ConditionalText = text;
	  } else {
	      free(text);
	  }
	}
	'}'
	[ ',' '{'
	  { text = ReadBody();
	    if (IsFalse(cond)) {
	        ConditionalText = text;
	    } else {
	        free(text);
	    }
	  }
	  '}'
	| /* allow empty else part */
	  { if (IsFalse(cond)) {
	        ConditionalText = Salloc("", 1);
	    }
	  }
	]
	')'
	;
		
variable_def
	{ struct idf *idp;
	  struct expr *val;
	  extern struct idf *Id_dummy_var;
	}:
	[ %if (read_ahead() == '=')
	  ID
	  { idp = Tok.t_idp;
	    DeclareIdent(&idp, I_VARIABLE);
	  }
	  '='
	| /* empty */
	  { idp = Id_dummy_var; }
	]
	expression(MIN_PRIO, &val)
	{ EnterVarDef(idp, val);
	  if (ReadingCache) {
		idp->id_variable->var_flags |= V_STATEFILE;
	  } else if (idp == Id_dummy_var) { /* force immediate execution */
		StartExecution((struct job *)NULL);
	  }
	}
	;

cmd_line_def
	{ struct idf *idp;
	  struct expr *val;
	}:
	ID
	{ idp = Tok.t_idp;
	  DeclareIdent(&idp, I_VARIABLE);
	}
	[ '=' expression(MIN_PRIO, &val)
	| /* empty */ { val = TrueExpr; }
	]
	{ EnterVarDef(idp, val);
	  idp->id_variable->var_flags |= V_CMDLINE;	  
	}
	;

tool_def{ struct tool *tool;}:
	TOOL
	ID
	{ DeclareIdent(&Tok.t_idp, I_TOOL);
	  tool = Tok.t_idp->id_tool;
	  NewScope();
	  InlineExpansion = FALSE;
	}
	param_decl
	{ tool->tl_scope = CurrentScope;
	  PrintScope(tool->tl_scope);
	}
	[ CONDITION 	expression(MIN_PRIO, &tool->tl_condition) ';' ]?
	[ TRIGGER	expression(MIN_PRIO, &tool->tl_trigger)	  ';' ]?
	[ PRE		expression(MIN_PRIO, &tool->tl_pre)	  ';' ]?
	[ POST		expression(MIN_PRIO, &tool->tl_post)	  ';' ]?
	'{'
	statement_list(&tool->tl_action)		
	'}'
	{ ClearScope();
	  EnterTool(tool);
	  InlineExpansion = TRUE;
	}
	;

statement_list(struct expr **s;)
	{ struct expr *top_node, *new; }:
	[ [ expression(MIN_PRIO, &top_node) ';'
	    [ expression(MIN_PRIO, &new) ';'
	      { register struct expr *new_top = new_expr(AND);

		new_top->e_type = T_BOOLEAN;
		new_top->e_left = top_node;
		new_top->e_right = new;
		top_node = new_top;
	      }
	    ]*
	  ]
	| /*empty*/
	  { top_node = TrueExpr;}
	]
	{ *s = top_node; }
	;

param_decl:
	'(' [ option_io_decl ';' ]* ')'
	;

option_io_decl
	{ register struct param *par; }:
	ID
	{ DeclareIdent(&Tok.t_idp, I_PARAM);
	  par = Tok.t_idp->id_param;
	}
	':'
	type_spec(&par->par_type)
	[ %if (read_ahead() == COMPUTED)
	  ARROW COMPUTED REFID
	  { register struct idf *comp = Tok.t_idp;

	    if ((comp->id_param == NULL) ||
		(comp->id_param->par_type->tp_indicator & (T_OUT|T_OBJECT))
		 != (T_OUT|T_OBJECT)) { /* so no LIST_OF allowed? */
		SemanticIdError(comp, "OUT parameter name expected");
	    } else if ((par->par_type->tp_indicator & T_IN) == 0) {
		SemanticIdError(par->par_idp, "should be of input type");
	    } else {
		par->par_computed = comp; 
		par->par_flags |= PAR_COMPUTED;
		comp->id_param->par_flags |= PAR_REFCOMP;
	    }
	  }
	| ARROW
	  expr_designator(&par->par_default)
	| /* empty */
	]
	[ CONFORM REFID
	  { register struct idf *conf = Tok.t_idp;

	    if ((conf->id_param == NULL) ||
		(conf->id_param->par_type->tp_indicator & T_BOOLEAN) == 0) {
		SemanticIdError(conf, "boolean parameter name expected");
	    } else if ((par->par_type->tp_indicator & T_OBJECT) == 0) {
		SemanticIdError(par->par_idp, "should be of object type");
	    } else {
		par->par_conform = Tok.t_idp; 
		par->par_flags |= PAR_CONFORM;
	    }
	  }
	| /* empty */
	]
	;

type_spec(struct type **the_type;)
	{ register struct type *type = new_type(); }:
	[ BOOLEAN { type->tp_indicator |= T_BOOLEAN; }
	| STRING  { type->tp_indicator |= T_STRING; }
	| IN	    { type->tp_indicator |= T_IN|T_OBJECT; }
	| OUT	    { type->tp_indicator |= T_OUT|T_OBJECT; }
	| TMP	    { type->tp_indicator |= T_TMP|T_OBJECT; }
	]
	[ LIST	    { type->tp_indicator |= T_LIST_OF; }]?
	[ { if ((type->tp_indicator & T_OBJECT) == 0) {
		SemanticError("attributes only apply to objects");
	    }
	  }
	  attr_spec_list(&type->tp_attributes)
	| { if ((type->tp_indicator & T_OBJECT) != 0) {
		/* add empty attribute list! */
		type->tp_attributes = empty_list();
	    }
	  }
	]
	{ *the_type = type; }
	;

expression(int level; struct expr **expr;)
	{ int prio;
	  struct expr *operand, *operator;
	}:
	[ '/'	/* unix syntax: starting slash denotes $ROOT */
	  { operand = ObjectExpr(RootDir); }
	  /* if we now get a factor, we parse it as if a (real) slash operator
	   * would have been inserted: "/aap" is parsed the same as
	   * "$ROOT/aap".
	   */
	  [ %avoid
	  | factor(&operator)
	    { register struct expr *new = new_expr('/');

	      new->e_left = operand;
	      new->e_right = operator;
	      new->e_type = T_OBJECT;
	      operand = new;
	    }
	  ]
	| factor(&operand)
	]
	[ %while ((prio = priority[LLsymb]) >= level || prio == 0)
	  /* prio is 0 for non operator tokens, and -1 for '{'.
	   * The latter is to avoid parsing "ID '{'" as concatenation, when
	   * the '{' is meant to belong to a cluster.
	   * This is no real restriction because concatenation only works
	   * on strings anyway.
	   */
	  { operator = new_expr(LLsymb);
	    operator->e_left = operand;
	  }
	  [ %avoid
	    expression(priority['&'] + 1, &operator->e_right)
	    { operator->e_number = '&'; operator->e_type = T_STRING;
	      operand = operator;
	    }
	  | [ EQ	{ operator->e_type = T_BOOLEAN; }
	    | NOT_EQ 	{ operator->e_type = T_BOOLEAN; }
	    | AND	{ operator->e_type = T_BOOLEAN; }
	    | OR	{ operator->e_type = T_BOOLEAN; }
	    | '&'	{ operator->e_type = T_STRING;  }
	    | '+'	{ operator->e_type = T_LIST_OF | operand->e_type; }
	    | '?'	{ operator->e_type = T_ANY; }
	    | '\\'	{ operator->e_type = T_LIST_OF | operand->e_type; }
	    ]
	    expression(prio + 1, &operator->e_right)
	    { operand = operator; }
	  | '/'
	    [ %avoid /* empty */
	      { put_expr_node(operator); /* not used */ }
	    | expression(prio + 1, &operator->e_right)
	      {	operator->e_type = T_OBJECT;
	        operand = operator;
	      }
	    ]
	  | { operator = new_expr('[');	/* 'attach attribute' operator */
	      operator->e_left = operand;
	      operator->e_type = T_OBJECT;
	      if ((operand->e_type & T_LIST_OF) != 0) {
		  operator->e_type |= T_LIST_OF;
	      }
	    }
	    attr_spec_list(&operator->e_right)
	    { operand = operator; }
	  ]
	]*
	{ *expr = operand; }
	;


var_or_string(struct expr **expr;)
	{ struct expr *e; }:
	[ ID	/* mostly a pathname component */
	  { e = new_expr(ID_STRING);
	    e->e_idp = Tok.t_idp;
	    e->e_type = T_STRING;
	    /* This string may not be thrown away later on, it is in the
	     * symbol table.
	     */
	  }
	| STRING
	  { e = new_expr(STRING);
	    e->e_string = Tok.t_string;
	    e->e_type = T_STRING;
	  }
	| REFID
	  { struct idf *var_idp = Tok.t_idp;

	    if (var_idp->id_param != NULL) {	/* tool/deriv/cluster param */
		e = new_expr(PARAM);
		e->e_param = var_idp->id_param;
		e->e_type = e->e_param->par_type->tp_indicator;
	    } else { /* (global) variable reference */
		if (var_idp->id_variable != NULL &&
		    var_idp->id_variable->var_state == V_DONE &&
		    InlineExpansion) {
		    /* if variable already evaluated: expand it right away */
		    e = CopyExpr(var_idp->id_variable->var_expr);
		} else {
		    e = new_expr(REFID);
		    if ((e->e_variable = var_idp->id_variable) == NULL) {
		        Warning("undefined variable `%s' defaults to ''",
			        var_idp->id_text);
		        DeclareIdent(&var_idp, I_VARIABLE);
	                e->e_variable = var_idp->id_variable;
		        EnterVarDef(var_idp, StringExpr(""));
	            } else {
			if (e->e_variable->var_state == V_DONE) {
			    e->e_type = e->e_variable->var_expr->e_type;
			}
		    }
	        }
	    }
	  }
	]
	{ *expr = e; }
	;

factor(struct expr **expr;)
	{ struct expr *e, *new; }:
	[ %if (read_ahead() == '(') 
	  [ ID | IF ]
	  { CheckCallID(&Tok.t_idp);
	    e = MakeCallExpr(Tok.t_idp);
	  }
	  '(' arg_list(e, TRUE) ')'
	| var_or_string(&e)
	| INTEGER		{ e = IntExpr(Tok.t_integer); }
	| '(' expression(MIN_PRIO, &e) ')'
	| NOT
	  { e = new_expr(NOT); e->e_type = T_BOOLEAN; }
	  expression(priority[NOT], &e->e_left)
	| { e = NULL; }
	  '{' expression_list(&e)? ','? '}'
	  { if (e == NULL) {
		e = empty_list();
	    }
	  }
	| '"'	/* strings and variables within double quotes are catenated */
	  [ var_or_string(&e)
	    [ { new = new_expr('&');
		new->e_type = T_STRING;
		new->e_left = e;
	      }
	      var_or_string(&new->e_right)
	      { e = new; }
	    ]*
	  | /* empty */ { e = StringExpr(""); }
	  ]
	  '"'
	| TTRUE	  { e = TrueExpr; }
	| FFALSE  { e = FalseExpr; }
	| UNKNOWN { e = UnKnown; }
	]
	{ *expr = e; }
	;

expression_list(struct expr **elist;)
	{ register struct expr *e = empty_list();
	  struct expr *elem;
	}:
	expression(MIN_PRIO, &elem)		{ Append(&e->e_list, elem); }
	{ e->e_type = T_LIST_OF | elem->e_type; }
	[ %while (read_ahead() != '}')
	  ',' expression(MIN_PRIO, &elem)	{ Append(&e->e_list, elem); }
	]*
	{ *elist = e; }
	;

arg_list(struct expr *call; int check;)
	{ struct expr *arg;
	  struct cons *next_cons;
	  /* next_arg is used for positional parameters */
	  int ret_type = 0, any_type = 0, count = 0;
	  struct idf *func = call->e_func;
	}:
	{ next_cons = Head(call->e_args); }
	[ arg_spec(call, &arg, &next_cons)
	  { CheckArg(func, ++count, arg, &ret_type, &any_type); }
	  [ ','
	    arg_spec(call, &arg, &next_cons)
	    { CheckArg(func, ++count, arg, &ret_type, &any_type); }
	  ]*
	| /* no arguments */
	]
	/* Check that all arguments are assigned or have a default. */
	{ if (check) {
		CheckAllArgsAssigned(call, &ret_type);
	  }
	  if (ret_type == 0) {	/* i.e., no RETURN directive used */
		switch (func->id_kind & I_CALL) {
		case I_TOOL:
		    ret_type = T_BOOLEAN; break;
		case I_FUNCTION:
		    if ((ret_type = func->id_function->fun_result) == T_ANY) {
			ret_type = (any_type != 0) ? any_type : T_STRING;
		    }
		    break;
		default:
		    ret_type = T_BOOLEAN;
		 /* using yet unknown tools not dealt with correctly? */
		}
	  }
	  if (call->e_type != T_ERROR) {
		call->e_type = ret_type;
	  }
	}
	;


arg_spec(struct expr *call, **the_arg; struct cons **next_cons;)
	{ struct expr *arg; }:
	[ %if (read_ahead() == ARROW)
	  ID
	  /* no more positional parameters allowed */
	  { arg = ArgPlace(call, Tok.t_idp);
	    *next_cons = NULL;
	  }
	  ARROW
	| { if ((call->e_func->id_kind & I_ERROR) == 0) {
		if (*next_cons == NULL) {
		    call->e_type = T_ERROR; /* will be dealt with later */
		    if (!ReadingCache) {
			FuncError(call, "no positional argument allowed here");
		    }
		    AddParam(call, dummy_param());
		    arg = ItemP(TailOf(call->e_args), expr);
	        } else {
		    arg = ItemP(*next_cons, expr);
		    *next_cons = Next(*next_cons);
	        }
	    } else {
		AddParam(call, dummy_param());
		arg = ItemP(TailOf(call->e_args), expr);
	    }
	  }
	]
	{ if (arg->e_right != NULL) {
		FuncError(call, "parameter was already assigned to");
	  }
	}
	expr_designator(&arg->e_right)
	{ *the_arg = arg; }
	;

/*
 * In function/tool calls also the 'designators' IGNORE, RETURN and DIAG
 * may be used to affect its workings.
 */
expr_designator(struct expr **expr;):
	[ { *expr = new_expr(LLsymb);
	    (*expr)->e_type = T_SPECIAL;
	  }
	  [ IGNORE | RETURN | DIAG ]
	| expression(MIN_PRIO, expr)
	]
	;

declaration
	{ struct expr *elist; }:
	DECLARE
	expression_list(&elist)
	{ /* check that the top nodes are attribute attachments */
	  ITERATE(elist->e_list, expr, decl, {
	     if (decl->e_number != '[') {
	         SemanticError("declaration without attribute specification");
		 break;
	     }
	  });
	  EnterDeclaration(elist);
	}
	;

inclusion
	{ struct expr *elist; }:
	INCLUDE	expression_list(&elist)
	{ EnterInclusion(elist); }
	;

default_def
	{ struct expr *elist; }:
	DEFAULT expression_list(&elist)
	{ EnterDefault(elist); }
	;

import	{ struct idf *idp; }:
	IMPORT ID
	{ idp = Tok.t_idp;
	  DeclareIdent(&idp, I_VARIABLE);
	}
	[ STRING	{ DoImport(idp, Tok.t_string); }
	| /* empty */	{ DoImport(idp, ELEM_SEPARATOR); }
	]
	;

export	{ struct idf *idp; }:
	EXPORT ID
	{ idp = Tok.t_idp;
	  DeclareIdent(&idp, I_VARIABLE);
	}
	[ STRING	{ DoExport(idp, Tok.t_string); }
	| /* empty */	{ DoExport(idp, ELEM_SEPARATOR); }
	]
	;
	
derivation
	{ struct derivation *deriv = new_derivation(); }:
	{ NewScope(); }
	DERIVE
	ID
	{ DeclareIdent(&Tok.t_idp, I_PARAM);
	  Tok.t_idp->id_param->par_type = ObjectType;
	  deriv->dr_var = CurrentScope;
	}
	attr_spec_list(&deriv->dr_attr_spec)	 {PrintScope(deriv->dr_var); }
	{ EnterDerivation(deriv); }
	WHEN
	expression(MIN_PRIO, &deriv->dr_condition)
	{ ClearScope();	}
	;

attr_spec_list(struct expr **attr_list;)
	{ struct expr *attr; }:
	'['
	{ *attr_list = empty_list(); }
	[
	  attr_spec(&attr)
	  { Append(&(*attr_list)->e_list, attr); }
	  [ ','
	    attr_spec(&attr)
	    { Append(&(*attr_list)->e_list, attr); }
	  ]*
	| /* no attributes specified */
	]
	']'
	;


attr_spec(struct expr **attr;):
	/*
	 * An attribute spec "A = E" results in the expression '=>'(A,E)
	 * and attr spec "A" is translated into '=>'(A,TRUE).
	 */
	ID
	{ *attr = new_expr('=');
	  ((*attr)->e_left = new_expr(ID))->e_idp = Tok.t_idp;
	    DeclareAttribute(Tok.t_idp); /* possibly new attribute */
	}
	[ '=' expression(MIN_PRIO, &(*attr)->e_right) /* disallow ',' */
	| { (*attr)->e_right = TrueExpr; }
	]
	;

{
PRIVATE struct idf *
AnonCluster()
{
    static int n_unnamed = 0;
    static char anonymous[] = "_NoName-XXX";

    (void)sprintf(strchr(anonymous, '-') + 1, "%03d", n_unnamed++);
    return(str2idf(anonymous, 1));
}
}

cluster_def
	{ struct cluster *clus;
	  struct idf *clus_id;
	  struct expr *e, *name = NULL;
	  int anonymous;
	}:
	CLUSTER
	[ %if (LLsymb != '{')
	  /* resolves conflict between cluster '{' and expression '{' */
	  expression(MIN_PRIO, &name)
	  { if (name->e_number == ID_STRING) {
		clus_id = name->e_idp;
		/* put_expr(name); */	name = NULL;
	    } else {
		clus_id = AnonCluster(); /* real name put there later on */
	    }
	    anonymous = FALSE;
	  }
	| /* empty */
	  { clus_id = AnonCluster(); anonymous = TRUE; }
	]
	{ DeclareIdent(&clus_id, I_CLUSTER);
	  clus = clus_id->id_cluster;
	  if (anonymous) clus->cl_flags |= C_ANONYMOUS;
	  clus->cl_ident = name;
	  NewScope();
	  clus->cl_scope = CurrentScope; PrintScope(clus->cl_scope);
	}
	'{'
	[ TARGETS expression_list(&clus->cl_targets) ';'
	| /* empty */ { clus->cl_targets = empty_list(); }
	]
	[ SOURCES	expression_list(&clus->cl_sources) ';'
	| /* empty */ { clus->cl_sources = empty_list(); }
	]
	[ USE
	  tool_usage(&e)	{ Append(&clus->cl_invocations, e); }
	  [ ','
	    tool_usage(&e)	{ Append(&clus->cl_invocations, e); }
	  ]*
	  ';'
	| DO
	  statement_list(&clus->cl_command)
	  { clus->cl_flags |= C_IMPERATIVE; }
	| /* empty, use defaults */
	]
	'}'
	{ EnterCluster(clus);
	  ClearScope();
	}
	;

tool_usage(struct expr **inv;)	/* Looks like an (incomplete) toolinvocation */
	{ struct expr *e; }:
	ID
	{ CheckCallID(&Tok.t_idp);
	  if ((Tok.t_idp->id_kind & I_TOOL) == 0) {
		SemanticIdError(Tok.t_idp, "tool name expected");
	  }
	  e = MakeCallExpr(Tok.t_idp);
	}
	'(' arg_list(e, FALSE) ')'
	/* Don't check if all params have been assigned to. */
	{ *inv = e; }
	;

state_file:
	/* The statefile contains, among others, assignments to the
	 * variable ``_''.  They cause addition of an entry to the cache.
	 */
	[ variable_def ';' ]*
	;
		  
	
