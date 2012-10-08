/*	@(#)objects.h	1.2	94/04/07 14:53:06 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

EXTERN struct object *FirstObject;
/* head of the list of all internal objects */

struct object {
    struct idf		 *ob_idp;	 /* the name of the last component   */
    struct cached	 *ob_cache;	 /* cached invocations having this   */
					 /* object as first input	     */
    short		  ob_flags;
    short		  ob_index;	 /* objects are number from 0 onward */
    short		  ob_attr_alloc; /* # attributes allocated 	     */
    short		  ob_attr_max;	 /* index of last attribute 	     */
    struct object	 *ob_parent;	 /* The parent object (dir/lib/etc.) */
    struct object	 *ob_calias;	 /* current `alias' (link in cache)  */
    struct object	 *ob_same_comp;  /* object with same final component */
    struct object	 *ob_next;	 /* all objects are linked, too      */
    struct expr          *ob_expr;	 /* expression denoting this object  */
    struct slist	 *ob_working;    /* jobs working on it		     */
    char		 ob_attr_bits[8];/* bitset of attributes allocated   */
    struct attr_value   **ob_attr_array; /* extendable array of attr_values, */
					 /* consisting of (scope_nr, value)  */
					 /* mappings	     		     */
} *new_object P(( void ));

/* allocation definitions of struct object */
#include "structdef.h"
DEF_STRUCT(struct object, h_object, cnt_object)
#define new_object()      NEW_STRUCT(struct object, h_object, cnt_object, 50)
#define free_object(p)    FREE_STRUCT(struct object, h_object, p)

/* attribute descriptor, linked in symbol table for each attribute */
struct attr_descr {
    int 	  ad_index;	  /* index in attribute array, or -1 */
    struct slist *ad_derivations; /* derivations for this attribute */
};

/* allocation definitions of struct attr_descr */
DEF_STRUCT(struct attr_descr, h_attr_descr, cnt_attr_descr)
#define new_attr_descr()   NEW_STRUCT(struct attr_descr, h_attr_descr, cnt_attr_descr, 5)
#define free_attr_descr(p) FREE_STRUCT(struct attr_descr, h_attr_descr, p)

#define O_TMP		0x001	/* Amake generated temporary file object   */
#define O_RETURN	0x002	/* file whose contents have to be returned */
#define O_DIAG		0x004	/* diagnostic output file 		   */

#define O_OUTPUT	0x008   /* currently under production 		   */
#define O_INPUT		0x010	/* currently used as source		   */
#define O_TEMP		0x020	/* user defined temporary file		   */

#define O_FAILED	0x040	/* don't use it as source, command failed  */
#define O_IS_INCL	0x080	/* avoid including file more than once     */
#define O_BEING_INCL	0x100	/* working on this one			   */

#define O_SOURCE	0x200	/* source of at least on cluster	   */
#define O_TOUCHED	0x400	/* object touched on the command line	   */
#define O_CREATED	0x800	/* directory already created		   */

#define O_DUMPED       0x1000	/* object assignment written to statefile  */
#define O_GLOBAL_CID   0x2000	/* global cid value of this object is used */
#define O_STATE_INPUT  0x4000	/* input of some invocation in statefile   */

#define O_ABSENT_OK    0x8000	/* object will not be generated because the
				 * conformance clause is false */

#define DeclObjectIsSource(obj)	 { obj->ob_flags |= O_SOURCE; }
#define SourceOfSomeCluster(obj) ((obj->ob_flags & O_SOURCE) != 0)

EXTERN int AttributeScopeNr;

EXTERN struct object *RootDir, *AmakeDir, *Context,
    		     *FooObject, *IgnoreFile, *Anonymous;

#ifdef DEBUG
#define PrintObject(obj) DoPrintObject(obj)
#else
#define PrintObject(obj)
#endif

/* Attributes used internally (some also externally): */
EXTERN struct idf
    *Id_defpat,
    *Id_base,
    *Id_contents,
    *Id_is_source,
    *Id_computed,	/* object used as computed input */
    *Id_changed,	/* used to fake object change while F_no_exec */
    *Id_installed,	/* boolean attribute: don't remove this object */
    *Id_cid,
    *Id_id,
    *Id_target,
    *Id_alias,
    *Id_failed,
    *Id_implicit,
    *Id_output,
    *Id_dir,
    *Id_exists,
    *Id_err_reported,
    *Id_size,
    *Id_seq_nr,
    *Id_include;

void AttrStructReport	P((void));
void DeclareAttribute	P((struct idf *attr ));
void DoPrintObject	P((struct object *obj ));
void MergeAttributes	P((struct object *from , struct object *to ));
void PutAttrValue	P((struct object *obj , struct idf *attr ,
			   struct expr *val , int scope ));
void AppendAttrValue	P((struct object *obj , struct idf *attr ,
			   struct expr *val , int scope ));
void RemoveAttributes	P((struct object *obj ));
void GlobalizeAttr	P((struct object *obj , struct idf *attr ));
void AddAttrList	P((struct expr *obj , struct slist *attrs ));
void InitAttributes	P((void));
void PutDiagFile	P((struct object *obj ));
void RemoveDiagFiles	P((void));
void InitVariants	P((void));
void object_init	P((void));
char *ObjectName	P((struct object *obj ));
char *SystemName	P((struct object *obj ));
char *WorkDir		P((void));

struct expr *GetAttrInScope	P((struct object *obj , struct idf *attr ,
				   int scope ));
struct expr *GetAttrValue	P((struct object *obj , struct idf *attr ));
struct expr *ObjectExpr		P((struct object *object ));
struct object *NewDiagFile	P((void));
struct object *NewReturnFile	P((void));
struct object *Variant		P((struct object *obj ));
struct object *ObjectWithinParent P((struct idf *comp, struct object *parent));
struct object *SystemNameToObject P((char *s ));
