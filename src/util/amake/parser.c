/*	@(#)parser.c	1.3	94/04/07 14:53:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* LLgen generated code from source /home/staff4/versto/project/Amake/parser.g */
#ifdef LL_DEBUG
#define LL_assert(x) if(!(x)) LL_badassertion("x",__FILE__,__LINE__)
#else
#define LL_assert(x)	/* nothing */
#endif

extern int LLsymb;

#define LL_SAFE(x)	/* Nothing */
#define LL_SSCANDONE(x)	if (LLsymb != x) LLerror(x); else
#define LL_SCANDONE(x)	if (LLsymb != x) LLerror(x); else
#define LL_NOSCANDONE(x) LLscan(x)
#ifdef LL_FASTER
#define LLscan(x)	if ((LLsymb = LL_LEXI()) != x) LLerror(x); else
#endif

# include "Lpars.h"

extern unsigned int LLscnt[];
extern unsigned int LLtcnt[];
extern int LLcsymb;

#define LLsdecr(d)	{LL_assert(LLscnt[d] > 0); LLscnt[d]--;}
#define LLtdecr(d)	{LL_assert(LLtcnt[d] > 0); LLtcnt[d]--;}
#define LLsincr(d)	LLscnt[d]++
#define LLtincr(d)	LLtcnt[d]++
#define LL_LEXI lexan
# line 20 "/home/staff4/versto/project/Amake/parser.g"

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

# line 52 "/home/staff4/versto/project/Amake/parser.g"

PUBLIC int ParsingConstruction;


PRIVATE int InlineExpansion = TRUE;




PRIVATE struct expr *NewInstance = NULL;
PRIVATE char *ConditionalText = NULL;
L0_amake_program (
) {
LLsincr(0);
for (;;) {
L_1 : {switch(LLcsymb) {
case /*  EOFILE  */ 0 : ;
break;
default:{int LL_1=LLnext(0);
;if (!LL_1) {
break;
}
else if (LL_1 & 1) goto L_1;}
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  INTEGER  */ 4 : ;
case /*  CLUSTER  */ 10 : ;
case /*  DECLARE  */ 14 : ;
case /*  DERIVE  */ 15 : ;
case /*  DEFAULT  */ 16 : ;
case /*  EXPORT  */ 19 : ;
case /*  FFALSE  */ 20 : ;
case /*  GENERIC  */ 21 : ;
case /*  IF  */ 22 : ;
case /*  IMPORT  */ 24 : ;
case /*  INCLUDE  */ 26 : ;
case /*  INSTANCE  */ 27 : ;
case /*  NOT  */ 29 : ;
case /*  STRING  */ 36 : ;
case /*  TOOL  */ 39 : ;
case /*  TTRUE  */ 41 : ;
case /*  UNKNOWN  */ 42 : ;
case /* '(' */ 52 : ;
case /* '{' */ 55 : ;
case /* '/' */ 59 : ;
case /* '"' */ 64 : ;
LLtincr(51);
# line 71 "/home/staff4/versto/project/Amake/parser.g"
{ ParsingConstruction = TRUE; }
switch(LLcsymb) {
case /*  TOOL  */ 39 : ;
L3_tool_def(
);
LLread();
break;
case /*  DECLARE  */ 14 : ;
L4_declaration(
);
break;
case /*  DERIVE  */ 15 : ;
L5_derivation(
);
break;
case /*  INCLUDE  */ 26 : ;
L6_inclusion(
);
break;
case /*  CLUSTER  */ 10 : ;
L7_cluster_def(
);
LLread();
break;
case /*  IF  */ 22 : ;
L8_conditional(
);
LLread();
break;
default:
L9_variable_def(
);
break;
case /*  GENERIC  */ 21 : ;
L10_generic_def(
);
LLread();
break;
case /*  INSTANCE  */ 27 : ;
L11_instantiation(
);
LLread();
break;
case /*  DEFAULT  */ 16 : ;
L12_default_def(
);
break;
case /*  IMPORT  */ 24 : ;
L13_import(
);
break;
case /*  EXPORT  */ 19 : ;
L14_export(
);
break;
}
LLtdecr(51);
LL_SCANDONE(';');
# line 87 "/home/staff4/versto/project/Amake/parser.g"
{ ParsingConstruction = FALSE;
	    if (NewInstance != NULL) {
	        MakeInstance(NewInstance); 
		NewInstance = NULL;
	    } else if (ConditionalText != NULL) {
		DoInsertText(ConditionalText);
		 
		ConditionalText = NULL;
	    }
	  }
LLread();
continue;
}
}
LLsdecr(0);
break;
}
}
L10_generic_def (
) {
# line 101 "/home/staff4/versto/project/Amake/parser.g"
 struct idf *idp;
	  struct Generic *gen;
	
LLtincr(52);
LLtincr(1);
LLtincr(54);
LLtincr(55);
LLtincr(56);
LL_SAFE(GENERIC);
LL_NOSCANDONE(ID);
# line 106 "/home/staff4/versto/project/Amake/parser.g"
{ idp = Tok.t_idp;
	  DeclareIdent(&idp, I_GENERIC);
	  gen = idp->id_generic;
	  NewScope();
	}
LLtdecr(52);
LL_NOSCANDONE('(');
LLread();
L_2: ;
switch(LLcsymb) {
case /*  ID  */ 1 : ;
LLtdecr(1);
LLtincr(53);
LL_SAFE(ID);
# line 112 "/home/staff4/versto/project/Amake/parser.g"
{ DeclareIdent(&Tok.t_idp, I_PARAM); }
LLread();
for (;;) {
L_4 : {switch(LLcsymb) {
case /* ')' */ 54 : ;
break;
default:{int LL_2=LLnext(44);
;if (!LL_2) {
break;
}
else if (LL_2 & 1) goto L_4;}
case /* ',' */ 53 : ;
LL_SAFE(',');
LL_NOSCANDONE(ID);
# line 113 "/home/staff4/versto/project/Amake/parser.g"
{ DeclareIdent(&Tok.t_idp, I_PARAM); }
LLread();
continue;
}
}
LLtdecr(53);
break;
}
break;
case /* ')' */ 54 : ;
L_3: ;
LLtdecr(1);
break;
default: if (LLskip()) goto L_2;
goto L_3;
}
# line 116 "/home/staff4/versto/project/Amake/parser.g"
{ gen->gen_scope = CurrentScope;
	  PrintScope(CurrentScope);
	}
LLtdecr(54);
LL_SSCANDONE(')');
LLtdecr(55);
LL_NOSCANDONE('{');
# line 121 "/home/staff4/versto/project/Amake/parser.g"
{ gen->gen_text = ReadBody(); }
LLtdecr(56);
LL_NOSCANDONE('}');
# line 123 "/home/staff4/versto/project/Amake/parser.g"
{ ClearScope();	}
}
L11_instantiation (
) {
# line 127 "/home/staff4/versto/project/Amake/parser.g"
 struct expr *inst;
	  int bad = FALSE;
	
LLtincr(52);
LLsincr(1);
LLtincr(54);
LL_SAFE(INSTANCE);
LL_NOSCANDONE(ID);
# line 132 "/home/staff4/versto/project/Amake/parser.g"
{ if ((Tok.t_idp->id_kind & I_GENERIC) == 0) {
	      SemanticIdError(Tok.t_idp, "not a generic");
	      Tok.t_idp->id_kind |= I_ERROR;
	      bad = TRUE;
	  }
	  inst = MakeCallExpr(Tok.t_idp);
	}
LLtdecr(52);
LL_NOSCANDONE('(');
LLsdecr(1);
L15_arg_list(
# line 139 "/home/staff4/versto/project/Amake/parser.g"
inst, TRUE);
LLtdecr(54);
LL_SCANDONE(')');
# line 140 "/home/staff4/versto/project/Amake/parser.g"
{ if (!bad) NewInstance = inst;	}
}
L8_conditional (
) {
# line 144 "/home/staff4/versto/project/Amake/parser.g"
 extern struct idf *Id_if;
	  char *text;
	  struct expr *cond;
	
LLsincr(2);
LLtincr(53);
LLtincr(55);
LLtincr(56);
LLtincr(53);
LLtincr(54);
LL_SAFE(IF);
LL_NOSCANDONE('(');
LLread();
LLsdecr(2);
L16_expression(
# line 148 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &cond);
# line 149 "/home/staff4/versto/project/Amake/parser.g"
{ EnterVarDef(Id_if, cond);
	  StartExecution((struct job *)NULL);
	  cond = Id_if->id_variable->var_expr =
		GetExprOfType(Id_if->id_variable->var_expr, T_BOOLEAN);
	}
LLtdecr(53);
LL_SCANDONE(',');
LLtdecr(55);
LL_NOSCANDONE('{');
# line 155 "/home/staff4/versto/project/Amake/parser.g"
{ text = ReadBody();
	  if (IsTrue(cond)) {
	      ConditionalText = text;
	  } else {
	      free(text);
	  }
	}
LLtdecr(56);
LL_NOSCANDONE('}');
LLread();
L_2: ;
switch(LLcsymb) {
case /* ',' */ 53 : ;
LLtdecr(53);
LLtincr(56);
LL_SAFE(',');
LL_NOSCANDONE('{');
# line 164 "/home/staff4/versto/project/Amake/parser.g"
{ text = ReadBody();
	    if (IsFalse(cond)) {
	        ConditionalText = text;
	    } else {
	        free(text);
	    }
	  }
LLtdecr(56);
LL_NOSCANDONE('}');
LLread();
break;
case /* ')' */ 54 : ;
L_3: ;
LLtdecr(53);
# line 173 "/home/staff4/versto/project/Amake/parser.g"
{ if (IsFalse(cond)) {
	        ConditionalText = Salloc("", 1);
	    }
	  }
break;
default: if (LLskip()) goto L_2;
goto L_3;
}
LLtdecr(54);
LL_SCANDONE(')');
}
L9_variable_def (
) {
# line 182 "/home/staff4/versto/project/Amake/parser.g"
 struct idf *idp;
	  struct expr *val;
	  extern struct idf *Id_dummy_var;
	
LLsincr(2);
switch(LLcsymb) {
case /*  ID  */ 1 : ;
# line 186 "/home/staff4/versto/project/Amake/parser.g"
if (!(read_ahead() == '=')) goto L_1;
LL_SAFE(ID);
# line 188 "/home/staff4/versto/project/Amake/parser.g"
{ idp = Tok.t_idp;
	    DeclareIdent(&idp, I_VARIABLE);
	  }
LL_NOSCANDONE('=');
LLread();
break;
L_1 : ;
default:
# line 193 "/home/staff4/versto/project/Amake/parser.g"
{ idp = Id_dummy_var; }
break;
}
LLsdecr(2);
L16_expression(
# line 195 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &val);
# line 196 "/home/staff4/versto/project/Amake/parser.g"
{ EnterVarDef(idp, val);
	  if (ReadingCache) {
		idp->id_variable->var_flags |= V_STATEFILE;
	  } else if (idp == Id_dummy_var) { 
		StartExecution((struct job *)NULL);
	  }
	}
}
L2_cmd_line_def (
) {
# line 206 "/home/staff4/versto/project/Amake/parser.g"
 struct idf *idp;
	  struct expr *val;
	
LLtincr(57);
LL_SCANDONE(ID);
# line 210 "/home/staff4/versto/project/Amake/parser.g"
{ idp = Tok.t_idp;
	  DeclareIdent(&idp, I_VARIABLE);
	}
LLread();
L_2: ;
switch(LLcsymb) {
case /* '=' */ 57 : ;
LLtdecr(57);
LL_SAFE('=');
LLread();
L16_expression(
# line 213 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &val);
break;
case /*  EOFILE  */ 0 : ;
L_3: ;
LLtdecr(57);
# line 214 "/home/staff4/versto/project/Amake/parser.g"
{ val = TrueExpr; }
break;
default: if (LLskip()) goto L_2;
goto L_3;
}
# line 216 "/home/staff4/versto/project/Amake/parser.g"
{ EnterVarDef(idp, val);
	  idp->id_variable->var_flags |= V_CMDLINE;	  
	}
}
L3_tool_def (
) {
# line 221 "/home/staff4/versto/project/Amake/parser.g"
 struct tool *tool;
LLsincr(3);
LLtincr(12);
LLtincr(40);
LLtincr(33);
LLtincr(32);
LLtincr(55);
LLsincr(4);
LLtincr(56);
LL_SAFE(TOOL);
LL_NOSCANDONE(ID);
# line 224 "/home/staff4/versto/project/Amake/parser.g"
{ DeclareIdent(&Tok.t_idp, I_TOOL);
	  tool = Tok.t_idp->id_tool;
	  NewScope();
	  InlineExpansion = FALSE;
	}
LLsdecr(3);
L17_param_decl(
);
# line 230 "/home/staff4/versto/project/Amake/parser.g"
{ tool->tl_scope = CurrentScope;
	  PrintScope(tool->tl_scope);
	}
LLread();
L_1 : {switch(LLcsymb) {
case /*  POST  */ 32 : ;
case /*  PRE  */ 33 : ;
case /*  TRIGGER  */ 40 : ;
case /* '{' */ 55 : ;
LLtdecr(12);
break;
default:{int LL_3=LLnext(268);
;if (!LL_3) {
LLtdecr(12);
break;
}
else if (LL_3 & 1) goto L_1;}
case /*  CONDITION  */ 12 : ;
LLtdecr(12);
LLtincr(51);
LL_SAFE(CONDITION);
LLread();
L16_expression(
# line 233 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &tool->tl_condition);
LLtdecr(51);
LL_SCANDONE(';');
LLread();
}
}
L_2 : {switch(LLcsymb) {
case /*  POST  */ 32 : ;
case /*  PRE  */ 33 : ;
case /* '{' */ 55 : ;
LLtdecr(40);
break;
default:{int LL_4=LLnext(296);
;if (!LL_4) {
LLtdecr(40);
break;
}
else if (LL_4 & 1) goto L_2;}
case /*  TRIGGER  */ 40 : ;
LLtdecr(40);
LLtincr(51);
LL_SAFE(TRIGGER);
LLread();
L16_expression(
# line 234 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &tool->tl_trigger);
LLtdecr(51);
LL_SCANDONE(';');
LLread();
}
}
L_3 : {switch(LLcsymb) {
case /*  POST  */ 32 : ;
case /* '{' */ 55 : ;
LLtdecr(33);
break;
default:{int LL_5=LLnext(289);
;if (!LL_5) {
LLtdecr(33);
break;
}
else if (LL_5 & 1) goto L_3;}
case /*  PRE  */ 33 : ;
LLtdecr(33);
LLtincr(51);
LL_SAFE(PRE);
LLread();
L16_expression(
# line 235 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &tool->tl_pre);
LLtdecr(51);
LL_SCANDONE(';');
LLread();
}
}
L_4 : {switch(LLcsymb) {
case /* '{' */ 55 : ;
LLtdecr(32);
break;
default:{int LL_6=LLnext(288);
;if (!LL_6) {
LLtdecr(32);
break;
}
else if (LL_6 & 1) goto L_4;}
case /*  POST  */ 32 : ;
LLtdecr(32);
LLtincr(51);
LL_SAFE(POST);
LLread();
L16_expression(
# line 236 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &tool->tl_post);
LLtdecr(51);
LL_SCANDONE(';');
LLread();
}
}
LLtdecr(55);
LL_SCANDONE('{');
LLsdecr(4);
L18_statement_list(
# line 238 "/home/staff4/versto/project/Amake/parser.g"
&tool->tl_action);
LLtdecr(56);
LL_SCANDONE('}');
# line 240 "/home/staff4/versto/project/Amake/parser.g"
{ ClearScope();
	  EnterTool(tool);
	  InlineExpansion = TRUE;
	}
}
L18_statement_list (
# line 246 "/home/staff4/versto/project/Amake/parser.g"
 s) struct expr **s; {
# line 247 "/home/staff4/versto/project/Amake/parser.g"
 struct expr *top_node, *new; 
LLsincr(4);
LLread();
L_2: ;
switch(LLcsymb) {
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  INTEGER  */ 4 : ;
case /*  FFALSE  */ 20 : ;
case /*  IF  */ 22 : ;
case /*  NOT  */ 29 : ;
case /*  STRING  */ 36 : ;
case /*  TTRUE  */ 41 : ;
case /*  UNKNOWN  */ 42 : ;
case /* '(' */ 52 : ;
case /* '{' */ 55 : ;
case /* '/' */ 59 : ;
case /* '"' */ 64 : ;
LLtincr(51);
L16_expression(
# line 248 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &top_node);
LLtdecr(51);
LL_SCANDONE(';');
LLread();
for (;;) {
L_4 : {switch(LLcsymb) {
case /* '}' */ 56 : ;
break;
default:{int LL_7=LLnext(-36);
;if (!LL_7) {
break;
}
else if (LL_7 & 1) goto L_4;}
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  INTEGER  */ 4 : ;
case /*  FFALSE  */ 20 : ;
case /*  IF  */ 22 : ;
case /*  NOT  */ 29 : ;
case /*  STRING  */ 36 : ;
case /*  TTRUE  */ 41 : ;
case /*  UNKNOWN  */ 42 : ;
case /* '(' */ 52 : ;
case /* '{' */ 55 : ;
case /* '/' */ 59 : ;
case /* '"' */ 64 : ;
LLtincr(51);
L16_expression(
# line 249 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &new);
LLtdecr(51);
LL_SCANDONE(';');
# line 250 "/home/staff4/versto/project/Amake/parser.g"
{ register struct expr *new_top = new_expr(AND);

		new_top->e_type = T_BOOLEAN;
		new_top->e_left = top_node;
		new_top->e_right = new;
		top_node = new_top;
	      }
LLread();
continue;
}
}
LLsdecr(4);
break;
}
break;
case /* '}' */ 56 : ;
L_3: ;
LLsdecr(4);
# line 260 "/home/staff4/versto/project/Amake/parser.g"
{ top_node = TrueExpr;}
break;
default: if (LLskip()) goto L_2;
goto L_3;
}
# line 262 "/home/staff4/versto/project/Amake/parser.g"
{ *s = top_node; }
}
L17_param_decl (
) {
LLtincr(1);
LLtincr(54);
LL_NOSCANDONE('(');
LLread();
for (;;) {
L_1 : {switch(LLcsymb) {
case /* ')' */ 54 : ;
break;
default:{int LL_8=LLnext(257);
;if (!LL_8) {
break;
}
else if (LL_8 & 1) goto L_1;}
case /*  ID  */ 1 : ;
LLtincr(51);
L19_option_io_decl(
);
LLtdecr(51);
LL_SCANDONE(';');
LLread();
continue;
}
}
LLtdecr(1);
break;
}
LLtdecr(54);
LL_SSCANDONE(')');
}
L19_option_io_decl (
) {
# line 270 "/home/staff4/versto/project/Amake/parser.g"
 register struct param *par; 
LLsincr(5);
LLtincr(7);
LLtincr(13);
LL_SAFE(ID);
# line 272 "/home/staff4/versto/project/Amake/parser.g"
{ DeclareIdent(&Tok.t_idp, I_PARAM);
	  par = Tok.t_idp->id_param;
	}
LL_NOSCANDONE(':');
LLsdecr(5);
L20_type_spec(
# line 276 "/home/staff4/versto/project/Amake/parser.g"
&par->par_type);
L_2: ;
switch(LLcsymb) {
case /*  ARROW  */ 7 : ;
# line 277 "/home/staff4/versto/project/Amake/parser.g"
if (!(read_ahead() == COMPUTED)) goto L_1;
LLtdecr(7);
LLtincr(3);
LL_SAFE(ARROW);
LL_NOSCANDONE(COMPUTED);
LLtdecr(3);
LL_NOSCANDONE(REFID);
# line 279 "/home/staff4/versto/project/Amake/parser.g"
{ register struct idf *comp = Tok.t_idp;

	    if ((comp->id_param == NULL) ||
		(comp->id_param->par_type->tp_indicator & (T_OUT|T_OBJECT))
		 != (T_OUT|T_OBJECT)) { 
		SemanticIdError(comp, "OUT parameter name expected");
	    } else if ((par->par_type->tp_indicator & T_IN) == 0) {
		SemanticIdError(par->par_idp, "should be of input type");
	    } else {
		par->par_computed = comp; 
		par->par_flags |= PAR_COMPUTED;
		comp->id_param->par_flags |= PAR_REFCOMP;
	    }
	  }
LLread();
break;
L_1 : ;
default:
switch(LLcsymb) {
case /*  ARROW  */ 7 : ;
LLtdecr(7);
LLsincr(1);
LL_SAFE(ARROW);
LLread();
L21_expr_designator(
# line 294 "/home/staff4/versto/project/Amake/parser.g"
&par->par_default);
break;
case /*  CONFORM  */ 13 : ;
case /* ';' */ 51 : ;
L_6: ;
LLtdecr(7);
break;
default: if (LLskip()) goto L_2;
goto L_6;
}
}
L_8: ;
switch(LLcsymb) {
case /*  CONFORM  */ 13 : ;
LLtdecr(13);
LL_SAFE(CONFORM);
LL_NOSCANDONE(REFID);
# line 298 "/home/staff4/versto/project/Amake/parser.g"
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
LLread();
break;
case /* ';' */ 51 : ;
L_9: ;
LLtdecr(13);
break;
default: if (LLskip()) goto L_8;
goto L_9;
}
}
L20_type_spec (
# line 314 "/home/staff4/versto/project/Amake/parser.g"
 the_type) struct type **the_type; {
# line 315 "/home/staff4/versto/project/Amake/parser.g"
 register struct type *type = new_type(); 
LLsincr(6);
LLtincr(28);
LLtincr(65);
LLread();
L_2: ;
switch(LLcsymb) {
case /*  BOOLEAN  */ 9 : ;
L_3: ;
LLsdecr(6);
LL_SSCANDONE(BOOLEAN);
# line 316 "/home/staff4/versto/project/Amake/parser.g"
{ type->tp_indicator |= T_BOOLEAN; }
break;
default: if (LLskip()) goto L_2;
goto L_3;
case /*  STRING  */ 36 : ;
LLsdecr(6);
LL_SAFE(STRING);
# line 317 "/home/staff4/versto/project/Amake/parser.g"
{ type->tp_indicator |= T_STRING; }
break;
case /*  IN  */ 25 : ;
LLsdecr(6);
LL_SAFE(IN);
# line 318 "/home/staff4/versto/project/Amake/parser.g"
{ type->tp_indicator |= T_IN|T_OBJECT; }
break;
case /*  OUT  */ 31 : ;
LLsdecr(6);
LL_SAFE(OUT);
# line 319 "/home/staff4/versto/project/Amake/parser.g"
{ type->tp_indicator |= T_OUT|T_OBJECT; }
break;
case /*  TMP  */ 38 : ;
LLsdecr(6);
LL_SAFE(TMP);
# line 320 "/home/staff4/versto/project/Amake/parser.g"
{ type->tp_indicator |= T_TMP|T_OBJECT; }
break;
}
LLread();
L_4 : {switch(LLcsymb) {
case /*  ARROW  */ 7 : ;
case /*  CONFORM  */ 13 : ;
case /* ';' */ 51 : ;
case /* '[' */ 65 : ;
LLtdecr(28);
break;
default:{int LL_9=LLnext(284);
;if (!LL_9) {
LLtdecr(28);
break;
}
else if (LL_9 & 1) goto L_4;}
case /*  LIST  */ 28 : ;
LLtdecr(28);
LL_SAFE(LIST);
# line 322 "/home/staff4/versto/project/Amake/parser.g"
{ type->tp_indicator |= T_LIST_OF; }
LLread();
}
}
L_6: ;
switch(LLcsymb) {
case /* '[' */ 65 : ;
LLtdecr(65);
# line 323 "/home/staff4/versto/project/Amake/parser.g"
{ if ((type->tp_indicator & T_OBJECT) == 0) {
		SemanticError("attributes only apply to objects");
	    }
	  }
L22_attr_spec_list(
# line 327 "/home/staff4/versto/project/Amake/parser.g"
&type->tp_attributes);
LLread();
break;
case /*  ARROW  */ 7 : ;
case /*  CONFORM  */ 13 : ;
case /* ';' */ 51 : ;
L_7: ;
LLtdecr(65);
# line 328 "/home/staff4/versto/project/Amake/parser.g"
{ if ((type->tp_indicator & T_OBJECT) != 0) {
		
		type->tp_attributes = empty_list();
	    }
	  }
break;
default: if (LLskip()) goto L_6;
goto L_7;
}
# line 334 "/home/staff4/versto/project/Amake/parser.g"
{ *the_type = type; }
}
L16_expression (
# line 337 "/home/staff4/versto/project/Amake/parser.g"
 level,expr) int level; struct expr **expr; {
# line 338 "/home/staff4/versto/project/Amake/parser.g"
 int prio;
	  struct expr *operand, *operator;
	
LLsincr(4);
LLsincr(2);
L_2: ;
switch(LLcsymb) {
case /* '/' */ 59 : ;
L_3: ;
LLsdecr(4);
LLsincr(7);
LL_SSCANDONE('/');
# line 342 "/home/staff4/versto/project/Amake/parser.g"
{ operand = ObjectExpr(RootDir); }
LLread();
L_5: ;
switch(LLcsymb) {
case /*  EOFILE  */ 0 : ;
case /*  EQ  */ 5 : ;
case /*  NOT_EQ  */ 6 : ;
case /*  AND  */ 8 : ;
case /*  CONFORM  */ 13 : ;
case /*  OR  */ 30 : ;
case /* ';' */ 51 : ;
case /* ',' */ 53 : ;
case /* ')' */ 54 : ;
case /* '}' */ 56 : ;
case /* '/' */ 59 : ;
case /* '&' */ 60 : ;
case /* '+' */ 61 : ;
case /* '?' */ 62 : ;
case /* '\\' */ 63 : ;
case /* '[' */ 65 : ;
case /* ']' */ 66 : ;
L_6: ;
LLsdecr(7);
break;
default: if (LLskip()) goto L_5;
goto L_6;
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  INTEGER  */ 4 : ;
case /*  FFALSE  */ 20 : ;
case /*  IF  */ 22 : ;
case /*  NOT  */ 29 : ;
case /*  STRING  */ 36 : ;
case /*  TTRUE  */ 41 : ;
case /*  UNKNOWN  */ 42 : ;
case /* '(' */ 52 : ;
case /* '{' */ 55 : ;
case /* '"' */ 64 : ;
LLsdecr(7);
L23_factor(
# line 348 "/home/staff4/versto/project/Amake/parser.g"
&operator);
# line 349 "/home/staff4/versto/project/Amake/parser.g"
{ register struct expr *new = new_expr('/');

	      new->e_left = operand;
	      new->e_right = operator;
	      new->e_type = T_OBJECT;
	      operand = new;
	    }
break;
}
break;
default: if (LLskip()) goto L_2;
goto L_3;
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  INTEGER  */ 4 : ;
case /*  FFALSE  */ 20 : ;
case /*  IF  */ 22 : ;
case /*  NOT  */ 29 : ;
case /*  STRING  */ 36 : ;
case /*  TTRUE  */ 41 : ;
case /*  UNKNOWN  */ 42 : ;
case /* '(' */ 52 : ;
case /* '{' */ 55 : ;
case /* '"' */ 64 : ;
LLsdecr(4);
L23_factor(
# line 357 "/home/staff4/versto/project/Amake/parser.g"
&operand);
break;
}
for (;;) {
L_7 : {switch(LLcsymb) {
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  INTEGER  */ 4 : ;
case /*  EQ  */ 5 : ;
case /*  NOT_EQ  */ 6 : ;
case /*  AND  */ 8 : ;
case /*  FFALSE  */ 20 : ;
case /*  IF  */ 22 : ;
case /*  NOT  */ 29 : ;
case /*  OR  */ 30 : ;
case /*  STRING  */ 36 : ;
case /*  TTRUE  */ 41 : ;
case /*  UNKNOWN  */ 42 : ;
case /* '(' */ 52 : ;
case /* '{' */ 55 : ;
case /* '/' */ 59 : ;
case /* '&' */ 60 : ;
case /* '+' */ 61 : ;
case /* '?' */ 62 : ;
case /* '\\' */ 63 : ;
case /* '"' */ 64 : ;
case /* '[' */ 65 : ;
# line 359 "/home/staff4/versto/project/Amake/parser.g"
if (((prio = priority[LLsymb]) >= level || prio == 0)) goto L_8;
case /*  EOFILE  */ 0 : ;
case /*  CONFORM  */ 13 : ;
case /* ';' */ 51 : ;
case /* ',' */ 53 : ;
case /* ')' */ 54 : ;
case /* '}' */ 56 : ;
case /* ']' */ 66 : ;
break;
default:{int LL_10=LLnext(-18);
;if (!LL_10) {
break;
}
else if (LL_10 & 1) goto L_7;}
L_8 : ;
# line 366 "/home/staff4/versto/project/Amake/parser.g"
{ operator = new_expr(LLsymb);
	    operator->e_left = operand;
	  }
switch(LLcsymb) {
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  INTEGER  */ 4 : ;
case /*  FFALSE  */ 20 : ;
case /*  IF  */ 22 : ;
case /*  NOT  */ 29 : ;
case /*  STRING  */ 36 : ;
case /*  TTRUE  */ 41 : ;
case /*  UNKNOWN  */ 42 : ;
case /* '(' */ 52 : ;
case /* '{' */ 55 : ;
case /* '"' */ 64 : ;
L16_expression(
# line 370 "/home/staff4/versto/project/Amake/parser.g"
priority['&'] + 1, &operator->e_right);
# line 371 "/home/staff4/versto/project/Amake/parser.g"
{ operator->e_number = '&'; operator->e_type = T_STRING;
	      operand = operator;
	    }
break;
case /*  EQ  */ 5 : ;
case /*  NOT_EQ  */ 6 : ;
case /*  AND  */ 8 : ;
case /*  OR  */ 30 : ;
case /* '&' */ 60 : ;
case /* '+' */ 61 : ;
case /* '?' */ 62 : ;
case /* '\\' */ 63 : ;
LLsincr(2);
switch(LLcsymb) {
default:
LL_SAFE(EQ);
# line 374 "/home/staff4/versto/project/Amake/parser.g"
{ operator->e_type = T_BOOLEAN; }
break;
case /*  NOT_EQ  */ 6 : ;
LL_SAFE(NOT_EQ);
# line 375 "/home/staff4/versto/project/Amake/parser.g"
{ operator->e_type = T_BOOLEAN; }
break;
case /*  AND  */ 8 : ;
LL_SAFE(AND);
# line 376 "/home/staff4/versto/project/Amake/parser.g"
{ operator->e_type = T_BOOLEAN; }
break;
case /*  OR  */ 30 : ;
LL_SAFE(OR);
# line 377 "/home/staff4/versto/project/Amake/parser.g"
{ operator->e_type = T_BOOLEAN; }
break;
case /* '&' */ 60 : ;
LL_SAFE('&');
# line 378 "/home/staff4/versto/project/Amake/parser.g"
{ operator->e_type = T_STRING;  }
break;
case /* '+' */ 61 : ;
LL_SAFE('+');
# line 379 "/home/staff4/versto/project/Amake/parser.g"
{ operator->e_type = T_LIST_OF | operand->e_type; }
break;
case /* '?' */ 62 : ;
LL_SAFE('?');
# line 380 "/home/staff4/versto/project/Amake/parser.g"
{ operator->e_type = T_ANY; }
break;
case /* '\\' */ 63 : ;
LL_SAFE('\\');
# line 381 "/home/staff4/versto/project/Amake/parser.g"
{ operator->e_type = T_LIST_OF | operand->e_type; }
break;
}
LLread();
LLsdecr(2);
L16_expression(
# line 383 "/home/staff4/versto/project/Amake/parser.g"
prio + 1, &operator->e_right);
# line 384 "/home/staff4/versto/project/Amake/parser.g"
{ operand = operator; }
break;
default:
LLsincr(4);
LL_SAFE('/');
LLread();
L_16: ;
switch(LLcsymb) {
case /*  EOFILE  */ 0 : ;
case /*  EQ  */ 5 : ;
case /*  NOT_EQ  */ 6 : ;
case /*  AND  */ 8 : ;
case /*  CONFORM  */ 13 : ;
case /*  OR  */ 30 : ;
case /* ';' */ 51 : ;
case /* ',' */ 53 : ;
case /* ')' */ 54 : ;
case /* '}' */ 56 : ;
case /* '&' */ 60 : ;
case /* '+' */ 61 : ;
case /* '?' */ 62 : ;
case /* '\\' */ 63 : ;
case /* '[' */ 65 : ;
case /* ']' */ 66 : ;
L_17: ;
LLsdecr(4);
# line 387 "/home/staff4/versto/project/Amake/parser.g"
{ put_expr_node(operator);  }
break;
default: if (LLskip()) goto L_16;
goto L_17;
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  INTEGER  */ 4 : ;
case /*  FFALSE  */ 20 : ;
case /*  IF  */ 22 : ;
case /*  NOT  */ 29 : ;
case /*  STRING  */ 36 : ;
case /*  TTRUE  */ 41 : ;
case /*  UNKNOWN  */ 42 : ;
case /* '(' */ 52 : ;
case /* '{' */ 55 : ;
case /* '/' */ 59 : ;
case /* '"' */ 64 : ;
LLsdecr(4);
L16_expression(
# line 388 "/home/staff4/versto/project/Amake/parser.g"
prio + 1, &operator->e_right);
# line 389 "/home/staff4/versto/project/Amake/parser.g"
{	operator->e_type = T_OBJECT;
	        operand = operator;
	      }
break;
}
break;
case /* '[' */ 65 : ;
# line 393 "/home/staff4/versto/project/Amake/parser.g"
{ operator = new_expr('[');	
	      operator->e_left = operand;
	      operator->e_type = T_OBJECT;
	      if ((operand->e_type & T_LIST_OF) != 0) {
		  operator->e_type |= T_LIST_OF;
	      }
	    }
L22_attr_spec_list(
# line 400 "/home/staff4/versto/project/Amake/parser.g"
&operator->e_right);
# line 401 "/home/staff4/versto/project/Amake/parser.g"
{ operand = operator; }
LLread();
break;
}
continue;
}
}
LLsdecr(2);
break;
}
# line 404 "/home/staff4/versto/project/Amake/parser.g"
{ *expr = operand; }
}
L24_var_or_string (
# line 408 "/home/staff4/versto/project/Amake/parser.g"
 expr) struct expr **expr; {
# line 409 "/home/staff4/versto/project/Amake/parser.g"
 struct expr *e; 
switch(LLcsymb) {
default:
LL_SAFE(ID);
# line 411 "/home/staff4/versto/project/Amake/parser.g"
{ e = new_expr(ID_STRING);
	    e->e_idp = Tok.t_idp;
	    e->e_type = T_STRING;
	    


	  }
break;
case /*  STRING  */ 36 : ;
LL_SAFE(STRING);
# line 419 "/home/staff4/versto/project/Amake/parser.g"
{ e = new_expr(STRING);
	    e->e_string = Tok.t_string;
	    e->e_type = T_STRING;
	  }
break;
case /*  REFID  */ 3 : ;
LL_SAFE(REFID);
# line 424 "/home/staff4/versto/project/Amake/parser.g"
{ struct idf *var_idp = Tok.t_idp;

	    if (var_idp->id_param != NULL) {	
		e = new_expr(PARAM);
		e->e_param = var_idp->id_param;
		e->e_type = e->e_param->par_type->tp_indicator;
	    } else { 
		if (var_idp->id_variable != NULL &&
		    var_idp->id_variable->var_state == V_DONE &&
		    InlineExpansion) {
		    
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
break;
}
# line 453 "/home/staff4/versto/project/Amake/parser.g"
{ *expr = e; }
}
L23_factor (
# line 456 "/home/staff4/versto/project/Amake/parser.g"
 expr) struct expr **expr; {
# line 457 "/home/staff4/versto/project/Amake/parser.g"
 struct expr *e, *new; 
switch(LLcsymb) {
case /*  ID  */ 1 : ;
# line 458 "/home/staff4/versto/project/Amake/parser.g"
if (!(read_ahead() == '(')) goto L_1;
case /*  IF  */ 22 : ;
LLtincr(52);
LLsincr(1);
LLtincr(54);
# line 460 "/home/staff4/versto/project/Amake/parser.g"
{ CheckCallID(&Tok.t_idp);
	    e = MakeCallExpr(Tok.t_idp);
	  }
LLtdecr(52);
LL_NOSCANDONE('(');
LLsdecr(1);
L15_arg_list(
# line 463 "/home/staff4/versto/project/Amake/parser.g"
e, TRUE);
LLtdecr(54);
LL_SCANDONE(')');
LLread();
break;
L_1 : ;
default:
switch(LLcsymb) {
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  STRING  */ 36 : ;
L24_var_or_string(
# line 464 "/home/staff4/versto/project/Amake/parser.g"
&e);
LLread();
break;
default:
LL_SAFE(INTEGER);
# line 465 "/home/staff4/versto/project/Amake/parser.g"
{ e = IntExpr(Tok.t_integer); }
LLread();
break;
case /* '(' */ 52 : ;
LLtincr(54);
LL_SAFE('(');
LLread();
L16_expression(
# line 466 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &e);
LLtdecr(54);
LL_SCANDONE(')');
LLread();
break;
case /*  NOT  */ 29 : ;
LL_SAFE(NOT);
# line 468 "/home/staff4/versto/project/Amake/parser.g"
{ e = new_expr(NOT); e->e_type = T_BOOLEAN; }
LLread();
L16_expression(
# line 469 "/home/staff4/versto/project/Amake/parser.g"
priority[NOT], &e->e_left);
break;
case /* '{' */ 55 : ;
LLsincr(4);
LLtincr(53);
LLtincr(56);
# line 470 "/home/staff4/versto/project/Amake/parser.g"
{ e = NULL; }
LL_SAFE('{');
LLread();
L_10 : {switch(LLcsymb) {
case /* ',' */ 53 : ;
case /* '}' */ 56 : ;
LLsdecr(4);
break;
default:{int LL_11=LLnext(-36);
;if (!LL_11) {
LLsdecr(4);
break;
}
else if (LL_11 & 1) goto L_10;}
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  INTEGER  */ 4 : ;
case /*  FFALSE  */ 20 : ;
case /*  IF  */ 22 : ;
case /*  NOT  */ 29 : ;
case /*  STRING  */ 36 : ;
case /*  TTRUE  */ 41 : ;
case /*  UNKNOWN  */ 42 : ;
case /* '(' */ 52 : ;
case /* '{' */ 55 : ;
case /* '/' */ 59 : ;
case /* '"' */ 64 : ;
LLsdecr(4);
L25_expression_list(
# line 471 "/home/staff4/versto/project/Amake/parser.g"
&e);
}
}
L_11 : {switch(LLcsymb) {
case /* '}' */ 56 : ;
LLtdecr(53);
break;
default:{int LL_12=LLnext(44);
;if (!LL_12) {
LLtdecr(53);
break;
}
else if (LL_12 & 1) goto L_11;}
case /* ',' */ 53 : ;
LLtdecr(53);
LL_SAFE(',');
LLread();
}
}
LLtdecr(56);
LL_SCANDONE('}');
# line 472 "/home/staff4/versto/project/Amake/parser.g"
{ if (e == NULL) {
		e = empty_list();
	    }
	  }
LLread();
break;
case /* '"' */ 64 : ;
LLsincr(8);
LLtincr(64);
LL_SAFE('"');
LLread();
L_13: ;
switch(LLcsymb) {
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  STRING  */ 36 : ;
L24_var_or_string(
# line 477 "/home/staff4/versto/project/Amake/parser.g"
&e);
LLread();
for (;;) {
L_15 : {switch(LLcsymb) {
case /* '"' */ 64 : ;
break;
default:{int LL_13=LLnext(-72);
;if (!LL_13) {
break;
}
else if (LL_13 & 1) goto L_15;}
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  STRING  */ 36 : ;
# line 478 "/home/staff4/versto/project/Amake/parser.g"
{ new = new_expr('&');
		new->e_type = T_STRING;
		new->e_left = e;
	      }
L24_var_or_string(
# line 482 "/home/staff4/versto/project/Amake/parser.g"
&new->e_right);
# line 483 "/home/staff4/versto/project/Amake/parser.g"
{ e = new; }
LLread();
continue;
}
}
LLsdecr(8);
break;
}
break;
case /* '"' */ 64 : ;
L_14: ;
LLsdecr(8);
# line 485 "/home/staff4/versto/project/Amake/parser.g"
{ e = StringExpr(""); }
break;
default: if (LLskip()) goto L_13;
goto L_14;
}
LLtdecr(64);
LL_SSCANDONE('"');
LLread();
break;
case /*  TTRUE  */ 41 : ;
LL_SAFE(TTRUE);
# line 488 "/home/staff4/versto/project/Amake/parser.g"
{ e = TrueExpr; }
LLread();
break;
case /*  FFALSE  */ 20 : ;
LL_SAFE(FFALSE);
# line 489 "/home/staff4/versto/project/Amake/parser.g"
{ e = FalseExpr; }
LLread();
break;
case /*  UNKNOWN  */ 42 : ;
LL_SAFE(UNKNOWN);
# line 490 "/home/staff4/versto/project/Amake/parser.g"
{ e = UnKnown; }
LLread();
break;
}
}
# line 492 "/home/staff4/versto/project/Amake/parser.g"
{ *expr = e; }
}
L25_expression_list (
# line 495 "/home/staff4/versto/project/Amake/parser.g"
 elist) struct expr **elist; {
# line 496 "/home/staff4/versto/project/Amake/parser.g"
 register struct expr *e = empty_list();
	  struct expr *elem;
	
LLtincr(53);
L16_expression(
# line 499 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &elem);
# line 499 "/home/staff4/versto/project/Amake/parser.g"
{ Append(&e->e_list, elem); }
# line 500 "/home/staff4/versto/project/Amake/parser.g"
{ e->e_type = T_LIST_OF | elem->e_type; }
for (;;) {
L_1 : {switch(LLcsymb) {
case /* ',' */ 53 : ;
# line 501 "/home/staff4/versto/project/Amake/parser.g"
if ((read_ahead() != '}')) goto L_2;
case /* ';' */ 51 : ;
case /* '}' */ 56 : ;
break;
default:{int LL_14=LLnext(44);
;if (!LL_14) {
break;
}
else if (LL_14 & 1) goto L_1;}
L_2 : ;
LL_SAFE(',');
LLread();
L16_expression(
# line 502 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &elem);
# line 502 "/home/staff4/versto/project/Amake/parser.g"
{ Append(&e->e_list, elem); }
continue;
}
}
LLtdecr(53);
break;
}
# line 504 "/home/staff4/versto/project/Amake/parser.g"
{ *elist = e; }
}
L15_arg_list (
# line 507 "/home/staff4/versto/project/Amake/parser.g"
 call,check) struct expr *call; int check; {
# line 508 "/home/staff4/versto/project/Amake/parser.g"
 struct expr *arg;
	  struct cons *next_cons;
	  
	  int ret_type = 0, any_type = 0, count = 0;
	  struct idf *func = call->e_func;
	
LLsincr(1);
# line 514 "/home/staff4/versto/project/Amake/parser.g"
{ next_cons = Head(call->e_args); }
LLread();
L_2: ;
switch(LLcsymb) {
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  INTEGER  */ 4 : ;
case /*  DIAG  */ 17 : ;
case /*  FFALSE  */ 20 : ;
case /*  IF  */ 22 : ;
case /*  IGNORE  */ 23 : ;
case /*  NOT  */ 29 : ;
case /*  RETURN  */ 34 : ;
case /*  STRING  */ 36 : ;
case /*  TTRUE  */ 41 : ;
case /*  UNKNOWN  */ 42 : ;
case /* '(' */ 52 : ;
case /* '{' */ 55 : ;
case /* '/' */ 59 : ;
case /* '"' */ 64 : ;
LLsdecr(1);
LLtincr(53);
L26_arg_spec(
# line 515 "/home/staff4/versto/project/Amake/parser.g"
call, &arg, &next_cons);
# line 516 "/home/staff4/versto/project/Amake/parser.g"
{ CheckArg(func, ++count, arg, &ret_type, &any_type); }
for (;;) {
L_4 : {switch(LLcsymb) {
case /* ')' */ 54 : ;
break;
default:{int LL_15=LLnext(44);
;if (!LL_15) {
break;
}
else if (LL_15 & 1) goto L_4;}
case /* ',' */ 53 : ;
LL_SAFE(',');
LLread();
L26_arg_spec(
# line 518 "/home/staff4/versto/project/Amake/parser.g"
call, &arg, &next_cons);
# line 519 "/home/staff4/versto/project/Amake/parser.g"
{ CheckArg(func, ++count, arg, &ret_type, &any_type); }
continue;
}
}
LLtdecr(53);
break;
}
break;
case /* ')' */ 54 : ;
L_3: ;
LLsdecr(1);
break;
default: if (LLskip()) goto L_2;
goto L_3;
}
# line 524 "/home/staff4/versto/project/Amake/parser.g"
{ if (check) {
		CheckAllArgsAssigned(call, &ret_type);
	  }
	  if (ret_type == 0) {	
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
		 
		}
	  }
	  if (call->e_type != T_ERROR) {
		call->e_type = ret_type;
	  }
	}
}
L26_arg_spec (
# line 548 "/home/staff4/versto/project/Amake/parser.g"
 call,the_arg,next_cons) struct expr *call, **the_arg; struct cons **next_cons; {
# line 549 "/home/staff4/versto/project/Amake/parser.g"
 struct expr *arg; 
LLtincr(1);
LLsincr(1);
L_2: ;
switch(LLcsymb) {
case /*  ID  */ 1 : ;
# line 550 "/home/staff4/versto/project/Amake/parser.g"
if (!(read_ahead() == ARROW)) goto L_1;
LLtdecr(1);
LL_SAFE(ID);
# line 553 "/home/staff4/versto/project/Amake/parser.g"
{ arg = ArgPlace(call, Tok.t_idp);
	    *next_cons = NULL;
	  }
LL_NOSCANDONE(ARROW);
LLread();
break;
L_1 : ;
case /*  REFID  */ 3 : ;
case /*  INTEGER  */ 4 : ;
case /*  DIAG  */ 17 : ;
case /*  FFALSE  */ 20 : ;
case /*  IF  */ 22 : ;
case /*  IGNORE  */ 23 : ;
case /*  NOT  */ 29 : ;
case /*  RETURN  */ 34 : ;
case /*  STRING  */ 36 : ;
case /*  TTRUE  */ 41 : ;
case /*  UNKNOWN  */ 42 : ;
case /* '(' */ 52 : ;
case /* '{' */ 55 : ;
case /* '/' */ 59 : ;
case /* '"' */ 64 : ;
L_3: ;
LLtdecr(1);
# line 557 "/home/staff4/versto/project/Amake/parser.g"
{ if ((call->e_func->id_kind & I_ERROR) == 0) {
		if (*next_cons == NULL) {
		    call->e_type = T_ERROR; 
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
break;
default: if (LLskip()) goto L_2;
goto L_3;
}
# line 575 "/home/staff4/versto/project/Amake/parser.g"
{ if (arg->e_right != NULL) {
		FuncError(call, "parameter was already assigned to");
	  }
	}
L21_expr_designator(
# line 579 "/home/staff4/versto/project/Amake/parser.g"
&arg->e_right);
# line 580 "/home/staff4/versto/project/Amake/parser.g"
{ *the_arg = arg; }
}
L21_expr_designator (
# line 587 "/home/staff4/versto/project/Amake/parser.g"
 expr) struct expr **expr; {
L_2: ;
switch(LLcsymb) {
case /*  DIAG  */ 17 : ;
case /*  IGNORE  */ 23 : ;
case /*  RETURN  */ 34 : ;
L_3: ;
LLsdecr(1);
# line 588 "/home/staff4/versto/project/Amake/parser.g"
{ *expr = new_expr(LLsymb);
	    (*expr)->e_type = T_SPECIAL;
	  }
switch(LLcsymb) {
default:
LL_SSCANDONE(IGNORE);
break;
case /*  RETURN  */ 34 : ;
LL_SAFE(RETURN);
break;
case /*  DIAG  */ 17 : ;
LL_SAFE(DIAG);
break;
}
LLread();
break;
default: if (LLskip()) goto L_2;
goto L_3;
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  INTEGER  */ 4 : ;
case /*  FFALSE  */ 20 : ;
case /*  IF  */ 22 : ;
case /*  NOT  */ 29 : ;
case /*  STRING  */ 36 : ;
case /*  TTRUE  */ 41 : ;
case /*  UNKNOWN  */ 42 : ;
case /* '(' */ 52 : ;
case /* '{' */ 55 : ;
case /* '/' */ 59 : ;
case /* '"' */ 64 : ;
LLsdecr(1);
L16_expression(
# line 592 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, expr);
break;
}
}
L4_declaration (
) {
# line 597 "/home/staff4/versto/project/Amake/parser.g"
 struct expr *elist; 
LL_SAFE(DECLARE);
LLread();
L25_expression_list(
# line 599 "/home/staff4/versto/project/Amake/parser.g"
&elist);
# line 600 "/home/staff4/versto/project/Amake/parser.g"
{ 
	  ITERATE(elist->e_list, expr, decl, {
	     if (decl->e_number != '[') {
	         SemanticError("declaration without attribute specification");
		 break;
	     }
	  });
	  EnterDeclaration(elist);
	}
}
L6_inclusion (
) {
# line 612 "/home/staff4/versto/project/Amake/parser.g"
 struct expr *elist; 
LL_SAFE(INCLUDE);
LLread();
L25_expression_list(
# line 613 "/home/staff4/versto/project/Amake/parser.g"
&elist);
# line 614 "/home/staff4/versto/project/Amake/parser.g"
{ EnterInclusion(elist); }
}
L12_default_def (
) {
# line 618 "/home/staff4/versto/project/Amake/parser.g"
 struct expr *elist; 
LL_SAFE(DEFAULT);
LLread();
L25_expression_list(
# line 619 "/home/staff4/versto/project/Amake/parser.g"
&elist);
# line 620 "/home/staff4/versto/project/Amake/parser.g"
{ EnterDefault(elist); }
}
L13_import (
) {
# line 623 "/home/staff4/versto/project/Amake/parser.g"
 struct idf *idp; 
LLtincr(36);
LL_SAFE(IMPORT);
LL_NOSCANDONE(ID);
# line 625 "/home/staff4/versto/project/Amake/parser.g"
{ idp = Tok.t_idp;
	  DeclareIdent(&idp, I_VARIABLE);
	}
LLread();
L_2: ;
switch(LLcsymb) {
case /*  STRING  */ 36 : ;
LLtdecr(36);
LL_SAFE(STRING);
# line 628 "/home/staff4/versto/project/Amake/parser.g"
{ DoImport(idp, Tok.t_string); }
LLread();
break;
case /* ';' */ 51 : ;
L_3: ;
LLtdecr(36);
# line 629 "/home/staff4/versto/project/Amake/parser.g"
{ DoImport(idp, ELEM_SEPARATOR); }
break;
default: if (LLskip()) goto L_2;
goto L_3;
}
}
L14_export (
) {
# line 633 "/home/staff4/versto/project/Amake/parser.g"
 struct idf *idp; 
LLtincr(36);
LL_SAFE(EXPORT);
LL_NOSCANDONE(ID);
# line 635 "/home/staff4/versto/project/Amake/parser.g"
{ idp = Tok.t_idp;
	  DeclareIdent(&idp, I_VARIABLE);
	}
LLread();
L_2: ;
switch(LLcsymb) {
case /*  STRING  */ 36 : ;
LLtdecr(36);
LL_SAFE(STRING);
# line 638 "/home/staff4/versto/project/Amake/parser.g"
{ DoExport(idp, Tok.t_string); }
LLread();
break;
case /* ';' */ 51 : ;
L_3: ;
LLtdecr(36);
# line 639 "/home/staff4/versto/project/Amake/parser.g"
{ DoExport(idp, ELEM_SEPARATOR); }
break;
default: if (LLskip()) goto L_2;
goto L_3;
}
}
L5_derivation (
) {
# line 644 "/home/staff4/versto/project/Amake/parser.g"
 struct derivation *deriv = new_derivation(); 
LLsincr(9);
LLtincr(44);
LLsincr(2);
# line 645 "/home/staff4/versto/project/Amake/parser.g"
{ NewScope(); }
LL_SAFE(DERIVE);
LL_NOSCANDONE(ID);
# line 648 "/home/staff4/versto/project/Amake/parser.g"
{ DeclareIdent(&Tok.t_idp, I_PARAM);
	  Tok.t_idp->id_param->par_type = ObjectType;
	  deriv->dr_var = CurrentScope;
	}
LLread();
LLsdecr(9);
L22_attr_spec_list(
# line 652 "/home/staff4/versto/project/Amake/parser.g"
&deriv->dr_attr_spec);
# line 652 "/home/staff4/versto/project/Amake/parser.g"
{PrintScope(deriv->dr_var); }
# line 653 "/home/staff4/versto/project/Amake/parser.g"
{ EnterDerivation(deriv); }
LLtdecr(44);
LL_NOSCANDONE(WHEN);
LLread();
LLsdecr(2);
L16_expression(
# line 655 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &deriv->dr_condition);
# line 656 "/home/staff4/versto/project/Amake/parser.g"
{ ClearScope();	}
}
L22_attr_spec_list (
# line 659 "/home/staff4/versto/project/Amake/parser.g"
 attr_list) struct expr **attr_list; {
# line 660 "/home/staff4/versto/project/Amake/parser.g"
 struct expr *attr; 
LLtincr(1);
LLtincr(66);
LL_SCANDONE('[');
# line 662 "/home/staff4/versto/project/Amake/parser.g"
{ *attr_list = empty_list(); }
LLread();
L_2: ;
switch(LLcsymb) {
case /*  ID  */ 1 : ;
LLtdecr(1);
LLtincr(53);
L27_attr_spec(
# line 664 "/home/staff4/versto/project/Amake/parser.g"
&attr);
# line 665 "/home/staff4/versto/project/Amake/parser.g"
{ Append(&(*attr_list)->e_list, attr); }
for (;;) {
L_4 : {switch(LLcsymb) {
case /* ']' */ 66 : ;
break;
default:{int LL_16=LLnext(44);
;if (!LL_16) {
break;
}
else if (LL_16 & 1) goto L_4;}
case /* ',' */ 53 : ;
LL_SAFE(',');
LLread();
L27_attr_spec(
# line 667 "/home/staff4/versto/project/Amake/parser.g"
&attr);
# line 668 "/home/staff4/versto/project/Amake/parser.g"
{ Append(&(*attr_list)->e_list, attr); }
continue;
}
}
LLtdecr(53);
break;
}
break;
case /* ']' */ 66 : ;
L_3: ;
LLtdecr(1);
break;
default: if (LLskip()) goto L_2;
goto L_3;
}
LLtdecr(66);
LL_SSCANDONE(']');
}
L27_attr_spec (
# line 676 "/home/staff4/versto/project/Amake/parser.g"
 attr) struct expr **attr; {
LLtincr(57);
LL_SCANDONE(ID);
# line 682 "/home/staff4/versto/project/Amake/parser.g"
{ *attr = new_expr('=');
	  ((*attr)->e_left = new_expr(ID))->e_idp = Tok.t_idp;
	    DeclareAttribute(Tok.t_idp); 
	}
LLread();
L_2: ;
switch(LLcsymb) {
case /* '=' */ 57 : ;
LLtdecr(57);
LL_SAFE('=');
LLread();
L16_expression(
# line 686 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &(*attr)->e_right);
break;
case /* ',' */ 53 : ;
case /* ']' */ 66 : ;
L_3: ;
LLtdecr(57);
# line 687 "/home/staff4/versto/project/Amake/parser.g"
{ (*attr)->e_right = TrueExpr; }
break;
default: if (LLskip()) goto L_2;
goto L_3;
}
}
# line 691 "/home/staff4/versto/project/Amake/parser.g"

PRIVATE struct idf *
AnonCluster()
{
    static int n_unnamed = 0;
    static char anonymous[] = "_NoName-XXX";

    (void)sprintf(strchr(anonymous, '-') + 1, "%03d", n_unnamed++);
    return(str2idf(anonymous, 1));
}
L7_cluster_def (
) {
# line 704 "/home/staff4/versto/project/Amake/parser.g"
 struct cluster *clus;
	  struct idf *clus_id;
	  struct expr *e, *name = NULL;
	  int anonymous;
	
LLsincr(4);
LLtincr(55);
LLtincr(37);
LLtincr(35);
LLsincr(10);
LLtincr(56);
LL_SAFE(CLUSTER);
LLread();
L_2: ;
switch(LLcsymb) {
case /* '{' */ 55 : ;
# line 710 "/home/staff4/versto/project/Amake/parser.g"
if (!(LLsymb != '{')) goto L_1;
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  INTEGER  */ 4 : ;
case /*  FFALSE  */ 20 : ;
case /*  IF  */ 22 : ;
case /*  NOT  */ 29 : ;
case /*  STRING  */ 36 : ;
case /*  TTRUE  */ 41 : ;
case /*  UNKNOWN  */ 42 : ;
case /* '(' */ 52 : ;
case /* '/' */ 59 : ;
case /* '"' */ 64 : ;
LLsdecr(4);
L16_expression(
# line 712 "/home/staff4/versto/project/Amake/parser.g"
MIN_PRIO, &name);
# line 713 "/home/staff4/versto/project/Amake/parser.g"
{ if (name->e_number == ID_STRING) {
		clus_id = name->e_idp;
			name = NULL;
	    } else {
		clus_id = AnonCluster(); 
	    }
	    anonymous = FALSE;
	  }
break;
L_1 : ;
L_3: ;
LLsdecr(4);
# line 722 "/home/staff4/versto/project/Amake/parser.g"
{ clus_id = AnonCluster(); anonymous = TRUE; }
break;
default: if (LLskip()) goto L_2;
goto L_3;
}
# line 724 "/home/staff4/versto/project/Amake/parser.g"
{ DeclareIdent(&clus_id, I_CLUSTER);
	  clus = clus_id->id_cluster;
	  if (anonymous) clus->cl_flags |= C_ANONYMOUS;
	  clus->cl_ident = name;
	  NewScope();
	  clus->cl_scope = CurrentScope; PrintScope(clus->cl_scope);
	}
LLtdecr(55);
LL_SCANDONE('{');
LLread();
L_5: ;
switch(LLcsymb) {
case /*  TARGETS  */ 37 : ;
LLtdecr(37);
LLtincr(51);
LL_SAFE(TARGETS);
LLread();
L25_expression_list(
# line 732 "/home/staff4/versto/project/Amake/parser.g"
&clus->cl_targets);
LLtdecr(51);
LL_SCANDONE(';');
LLread();
break;
case /*  DO  */ 18 : ;
case /*  SOURCES  */ 35 : ;
case /*  USE  */ 43 : ;
case /* '}' */ 56 : ;
L_6: ;
LLtdecr(37);
# line 733 "/home/staff4/versto/project/Amake/parser.g"
{ clus->cl_targets = empty_list(); }
break;
default: if (LLskip()) goto L_5;
goto L_6;
}
L_8: ;
switch(LLcsymb) {
case /*  SOURCES  */ 35 : ;
LLtdecr(35);
LLtincr(51);
LL_SAFE(SOURCES);
LLread();
L25_expression_list(
# line 735 "/home/staff4/versto/project/Amake/parser.g"
&clus->cl_sources);
LLtdecr(51);
LL_SCANDONE(';');
LLread();
break;
case /*  DO  */ 18 : ;
case /*  USE  */ 43 : ;
case /* '}' */ 56 : ;
L_9: ;
LLtdecr(35);
# line 736 "/home/staff4/versto/project/Amake/parser.g"
{ clus->cl_sources = empty_list(); }
break;
default: if (LLskip()) goto L_8;
goto L_9;
}
L_11: ;
switch(LLcsymb) {
case /*  USE  */ 43 : ;
LLsdecr(10);
LLtincr(53);
LLtincr(51);
LL_SAFE(USE);
L28_tool_usage(
# line 739 "/home/staff4/versto/project/Amake/parser.g"
&e);
# line 739 "/home/staff4/versto/project/Amake/parser.g"
{ Append(&clus->cl_invocations, e); }
LLread();
for (;;) {
L_13 : {switch(LLcsymb) {
case /* ';' */ 51 : ;
break;
default:{int LL_17=LLnext(44);
;if (!LL_17) {
break;
}
else if (LL_17 & 1) goto L_13;}
case /* ',' */ 53 : ;
LL_SAFE(',');
L28_tool_usage(
# line 741 "/home/staff4/versto/project/Amake/parser.g"
&e);
# line 741 "/home/staff4/versto/project/Amake/parser.g"
{ Append(&clus->cl_invocations, e); }
LLread();
continue;
}
}
LLtdecr(53);
break;
}
LLtdecr(51);
LL_SSCANDONE(';');
LLread();
break;
case /*  DO  */ 18 : ;
LLsdecr(10);
LL_SAFE(DO);
L18_statement_list(
# line 745 "/home/staff4/versto/project/Amake/parser.g"
&clus->cl_command);
# line 746 "/home/staff4/versto/project/Amake/parser.g"
{ clus->cl_flags |= C_IMPERATIVE; }
break;
case /* '}' */ 56 : ;
L_12: ;
LLsdecr(10);
break;
default: if (LLskip()) goto L_11;
goto L_12;
}
LLtdecr(56);
LL_SCANDONE('}');
# line 750 "/home/staff4/versto/project/Amake/parser.g"
{ EnterCluster(clus);
	  ClearScope();
	}
}
L28_tool_usage (
# line 755 "/home/staff4/versto/project/Amake/parser.g"
 inv) struct expr **inv; {
# line 756 "/home/staff4/versto/project/Amake/parser.g"
 struct expr *e; 
LLtincr(52);
LLsincr(1);
LLtincr(54);
LL_NOSCANDONE(ID);
# line 758 "/home/staff4/versto/project/Amake/parser.g"
{ CheckCallID(&Tok.t_idp);
	  if ((Tok.t_idp->id_kind & I_TOOL) == 0) {
		SemanticIdError(Tok.t_idp, "tool name expected");
	  }
	  e = MakeCallExpr(Tok.t_idp);
	}
LLtdecr(52);
LL_NOSCANDONE('(');
LLsdecr(1);
L15_arg_list(
# line 764 "/home/staff4/versto/project/Amake/parser.g"
e, FALSE);
LLtdecr(54);
LL_SCANDONE(')');
# line 766 "/home/staff4/versto/project/Amake/parser.g"
{ *inv = e; }
}
L1_state_file (
) {
LLsincr(4);
for (;;) {
L_1 : {switch(LLcsymb) {
case /*  EOFILE  */ 0 : ;
break;
default:{int LL_18=LLnext(-36);
;if (!LL_18) {
break;
}
else if (LL_18 & 1) goto L_1;}
case /*  ID  */ 1 : ;
case /*  REFID  */ 3 : ;
case /*  INTEGER  */ 4 : ;
case /*  FFALSE  */ 20 : ;
case /*  IF  */ 22 : ;
case /*  NOT  */ 29 : ;
case /*  STRING  */ 36 : ;
case /*  TTRUE  */ 41 : ;
case /*  UNKNOWN  */ 42 : ;
case /* '(' */ 52 : ;
case /* '{' */ 55 : ;
case /* '/' */ 59 : ;
case /* '"' */ 64 : ;
LLtincr(51);
L9_variable_def(
);
LLtdecr(51);
LL_SCANDONE(';');
LLread();
continue;
}
}
LLsdecr(4);
break;
}
}


