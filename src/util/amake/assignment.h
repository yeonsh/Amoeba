/*	@(#)assignment.h	1.2	94/04/07 14:46:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* variable states: */
enum var_state { V_ALLOC,	/* just allocated */
		 V_INIT,	/* has been entered */
		 V_BUSY,	/* variable being evaluated */
		 V_DONE,	/* expr evaluated */
		 V_AGAIN	/* being variable assigned again */
	       };

/* variable flags: */
#define V_BUILTIN   0x001 /* has a builtin interpretation */
#define V_INLINE    0x002 /* must be expanded inline */
#define V_CMDLINE   0x004 /* was assigned to on the commandline */
#define V_FROM_ENV  0x008 /* definition was generated from environment */
#define V_IN_ENV    0x010 /* is put in Amake's own environment for commands */
#define V_STATEFILE 0x020 /* statefile vars are not put in the environment*/
#define V_IMPORT    0x040 /* is imported */
#define V_EXPORT    0x080 /* is exported */
#define V_SHARED    0x100 /* is shared and used as tool default */

#define ELEM_SEPARATOR	":"
/* Default string separating elements in environment value.
 * For example: PATH=:/bin:/usr/bin is parsed as { '', /bin, /usr/bin }.
 * %import and %export allow a different separator, though.
 */

struct variable {
    struct idf		*var_idp;	/* variable name */
    enum var_state	 var_state;
    short                var_flags;
    short		 var_index;	/* index in environment */
    char		*var_separator;	/* environment <-> Amake conversion */
    struct expr		*var_expr;	/* the value of the variable */
    struct expr		*var_old_value;	/* used in "t = $t + $t" */
    struct job		*var_eval;	/* the job evaluating it */
} *new_variable P(( void ));

/* allocation definitions of struct variable */
#include "structdef.h"
DEF_STRUCT(struct variable, h_variable, cnt_variable)
#define new_variable()        NEW_STRUCT(struct variable, h_variable, cnt_variable, 10)
#define free_variable(p)      FREE_STRUCT(struct variable, h_variable, p)

void DoInsertText	P((char *txt ));
void CmdLineDefine	P((char *def ));
void ExecuteCmdLineDefs P((void));
void ReadEnvironment	P((char *env []));
void EnterVarDef	P((struct idf *idp , struct expr *val ));
void DoAssignment	P((struct variable **var_defp ));
void DoImport		P((struct idf *idp , char *sep ));
void DoExport		P((struct idf *idp , char *sep ));
void InitAssignments	P((void));
int  IsDefined		P((struct idf *idp ));
struct expr *
     GetVarValue	P((struct idf *idp ));
