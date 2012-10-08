/*	@(#)builtin.h	1.2	94/04/07 14:47:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* ids of builtin variables: */
EXTERN struct idf *Id_ROOT, *Id_AMAKELIB;

typedef struct expr *((*builtin_func) P(( struct slist *params )));

struct function {
    struct idf		*fun_idp;		/* function name */
    struct scope	*fun_scope;		/* list of struct param's */
    int			fun_result;		/* result type */
    builtin_func	fun_code;		/* the actual code */
};

/* allocation definitions of struct function */
#include "structdef.h"
DEF_STRUCT(struct function, h_function, cnt_function)
#define new_function()	  NEW_STRUCT(struct function, h_function, cnt_function, 10)
#define free_function(p)  FREE_STRUCT(struct function, h_function, p)

void HandleDiagnostics	P((struct slist *arg_list , char *vproc , struct job *exec ));
void BuiltinVarUpdate	P((struct variable *var ));
void builtin_init	P((struct slist *lib_dirs ));
void get_exec_args	P((struct slist *arg_list , struct expr **prog ,
			   struct expr **args , struct expr **standin ,
			   struct expr **standout , struct expr **standerr ));
char *CmdObjectName	P((struct object *obj ));
char *command_string	P((struct slist *arg_list , char **redirect ));

struct expr *DoGetAttrValues P((struct expr **objp , struct idf *attr));
struct expr *GetContents     P((struct object *obj, int follow, int remove));
struct expr *ExecResult	     P((struct slist *arg_list, struct expr *status));

