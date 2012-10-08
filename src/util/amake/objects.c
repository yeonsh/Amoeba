/*	@(#)objects.c	1.3	96/02/27 13:06:30 */
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
#include "idf.h"
#include "expr.h"
#include "Lpars.h"
#include "scope.h"
#include "derive.h"
#include "slist.h"
#include "dump.h"
#include "dbug.h"
#include "execute.h"
#include "eval.h"
#include "type.h"
#include "error.h"
#include "main.h"
#include "conversion.h"
#include "os.h"
#include "objects.h"

PUBLIC struct object *FirstObject = NULL;

PUBLIC int AttributeScopeNr = GLOBAL_SCOPE;
/* if this is not equal to GLOBAL_SCOPE, a local attribute is tried first */

PRIVATE short count_bits[256];
/* count_bits[i] = number of bits in the binary representation of i */

/*
 * To avoid searching an attribute list each time, space for most used
 * attributes (the ones used internally, as well as 'type') is reserved in
 * an array allocated for each object. This saves memory as well.
 * The actual indices of attributes kept in the array are determined run-time,
 * and put in the attribute descriptor.
 * Attributes that don't fit are put on an overflow list.
 */

struct attr_value {
    short	       av_scope; /* scope for which this value counts */
    struct expr	      *av_value; /* the value itself */
    struct attr_value *av_next;  /* other (scope, value) combinations */
};

/* allocation definitions of struct attr_value */
#include "structdef.h"
DEF_STRUCT(struct attr_value, h_attr_value, cnt_attr_value)
#define new_attr_value()	NEW_STRUCT(struct attr_value, h_attr_value, cnt_attr_value, 50)
#define free_attr_value(p)      FREE_STRUCT(struct attr_value, h_attr_value, p)


#ifdef DEBUG
PUBLIC void
AttrStructReport()
{
    StructReport("attr_value", sizeof(struct attr_value),
		 cnt_attr_value, h_attr_value);
}
#endif

PRIVATE struct slist *ArrayAttributes = NULL;

PUBLIC void
DeclareAttribute(attr)
struct idf *attr;
{
    static int last_attr_index = 0;

    if (attr->id_attr_descr == NULL) { /* not yet declared, create descriptor*/
	attr->id_kind |= I_ATTRIBUTE;
	attr->id_attr_descr = new_attr_descr();
	attr->id_attr_descr->ad_index = last_attr_index++;
	Append(&ArrayAttributes, attr);
	DBUG_PRINT("extend", ("index of attr `%s' is %d", 
			      attr->id_text, last_attr_index));
    }
}

/*
 * Low level attribute functions.
 */

PRIVATE int get_attr_index();

#ifdef DEBUG

PRIVATE void
PrintAttrValues(attr, av)
struct idf *attr;
struct attr_value *av;
{
    DBUG_EXECUTE("obj", {
	DBUG_WRITE(("%s: ", attr->id_text));
	for (; av != NULL; av = av->av_next) {
	    if (av->av_scope != GLOBAL_SCOPE) {
		DBUG_WRITE((" [%d] ", av->av_scope));
	    }
	    DBUG_Expr(av->av_value);
	}
	DBUG_NL();
    });
}

PUBLIC void
DoPrintObject(obj)
struct object *obj;
{
    DBUG_PRINT("obj", ("Attributes of `%s':", SystemName(obj)));
    ITERATE(ArrayAttributes, idf, attr, {
	int index;

	if ((index = get_attr_index(obj, attr)) >= 0) {
	    PrintAttrValues(attr, obj->ob_attr_array[index]);
	}
    });
}
#endif /* DEBUG */

PUBLIC void
MergeAttributes(from, to)
struct object *from, *to;
/* Put all the attributes of `from' to `to'.
 * This is only a sensible operation when these objects are to be considered
 * the same from now on. Currently it is used in the implementation of
 * viewpaths.
 * Note that this only works when `from' is only referred to from
 * one place! I see no better solution currently, because I don't want to
 * give up the uniqueness of objects.
 */
{
    register struct attr_value *av;

    ITERATE(ArrayAttributes, idf, attr, {
	int index;

	if ((index = get_attr_index(from, attr)) >= 0) {
	    for (av = from->ob_attr_array[index];
		 av != NULL; av = av->av_next) {
		if (GetAttrInScope(to, attr, av->av_scope) == UnKnown) {
		    DBUG_PRINT("merge", ("merge attribute `%s' to `%s'",
					 attr->id_text, SystemName(to)));
		    PutAttrValue(to, attr, CopyExpr(av->av_value),
				 av->av_scope);
		}
	    }
	}
    });
    PrintObject(from);
    PrintObject(to);
}

PRIVATE void
PrintBits(bits)
char *bits;
{
    int i, bit;
    char bit_string[100], *p = bit_string;

    for (i = 0; i < 4; i++) {
	for (bit = 0; bit < 8; bit++) {
	    if (bits[i] & (1 << bit)) {
		*p++ = '1';
	    } else {
		*p++ = '0';
	    }
	}
	*p++ = ' ';
    }
    *p = 0;
    DBUG_WRITE((bit_string));
}

#define BYTE_NO(index)	((index) >> 3)
#define BIT_NO(index)	((index) & 0x07)

#define last_bits(obj, byte_no, bit_no) 	\
    count_bits[(obj)->ob_attr_bits[byte_no] & (0xff >> (8 - (bit_no)))]

#define prev_bits(obj, i)			\
    count_bits[(obj)->ob_attr_bits[i] & 0xff]

PRIVATE int
count_nr_bits_before(obj, index)
struct object *obj;
int index;
{
    /* determine how many attributes are allocated with index smaller
     * than `index'
     */
    register int byte_no = BYTE_NO(index);
    register int bit_no = BIT_NO(index);
    register short i;
    register int bit_index = 0;

    for (i = 0; i < byte_no; i++) {
	bit_index += prev_bits(obj, i);
    }
    return(bit_index + last_bits(obj, byte_no, bit_no));
}

#define nr_bits_before(obj, index)					\
    ((index) < 8 ) ?  (last_bits(obj, BYTE_NO(index), BIT_NO(index))) :	\
    (((index) < 16) ? (last_bits(obj, BYTE_NO(index), BIT_NO(index)) +	\
		       prev_bits(obj, 0)) : 				\
     count_nr_bits_before(obj, index))

PRIVATE int
get_attr_index(obj, attr)
register struct object *obj;
register struct idf *attr;
/* Look for attribute 'attr' attached to object 'obj'.
 * Return the index of the attribute value list, or NULL if not found
 */
{
    register int index;

    assert(attr->id_attr_descr != NULL);

    if ((index = attr->id_attr_descr->ad_index) <= obj->ob_attr_max) {
	register int byte_no = BYTE_NO(index);
	register int bit_no = BIT_NO(index);

	if ((obj->ob_attr_bits[byte_no] & (1 << bit_no) ) != 0) {
	    return(nr_bits_before(obj, index));
	}
    }
    return(-1);
}

#define set_attr_bit(obj, attr) \
    {	register int index = attr->id_attr_descr->ad_index;		\
	obj->ob_attr_bits[BYTE_NO(index)] |= (1 << BIT_NO(index));	\
    }

#if !defined(__STDC__) && !defined(AMOEBA)
#define memmove(to, from, n)	(void)bcopy(from, to, n)
#endif

PUBLIC void
PutAttrValue(obj, attr, val, scope)
struct object	*obj;
struct idf	*attr;
struct expr	*val;
int		scope;
{
    register struct attr_value *av, *save;
    register struct attr_value **av_addr;
    register int index;

    /*
     * Add the attribute value 'attr_value' in scope 'scope' to attribute
     * 'attr'. The list is kept sorted from low to high scope nrs (reduces
     * searching by half).
     */

    if ((index = get_attr_index(obj, attr)) < 0) {
	/* make room for new attribute and insert it */
	unsigned int bytes;
	register int attr_index = attr->id_attr_descr->ad_index;

	if (obj->ob_attr_alloc == 0) {
	    bytes = sizeof(struct attr_value *);
	    obj->ob_attr_array = (struct attr_value **)Malloc(bytes);
	    obj->ob_attr_alloc = 1;
	    index = 0;
	    obj->ob_attr_max = attr_index;
	    DBUG_PRINT("extend", ("`%s' got first attribute",
				  obj->ob_idp->id_text));
	} else {
	    bytes = sizeof(struct attr_value *) * (obj->ob_attr_alloc + 1);

	    obj->ob_attr_array =
		(struct attr_value **)Realloc((char*)obj->ob_attr_array,bytes);

	    if (attr_index < obj->ob_attr_max) {
		index = nr_bits_before(obj, attr_index);
		/* move the attributes with higher index */
		memmove(&obj->ob_attr_array[index+1],
			&obj->ob_attr_array[index],
		        (obj->ob_attr_alloc-index)*sizeof(struct attr_value*));
	    } else {
		/* new highest index */
		obj->ob_attr_max = attr_index;
		index = obj->ob_attr_alloc;
	    }
	    obj->ob_attr_alloc += 1;

	    DBUG_PRINT("extend", ("`%s' now has %d entries",
				  obj->ob_idp->id_text, obj->ob_attr_alloc));
	}
	obj->ob_attr_array[index] = NULL;

	set_attr_bit(obj, attr);
    }

    DBUG_EXECUTE("showbits", {
	DBUG_WRITE(("attr bits: ")); PrintBits(obj->ob_attr_bits); DBUG_NL();
    });

    av_addr = &obj->ob_attr_array[index];

    /* look for scope 'scope' */
    save = NULL;
    for (av = *av_addr; av != NULL && av->av_scope < scope; av = av->av_next) {
	save = av;
    }

    if (av != NULL && av->av_scope == scope) {
	/* The attribute is already present, so replace it.
	 * This fact is currently used in replacing BUSY nodes.
	 */
	put_expr(av->av_value);
	av->av_value = val;
    } else {
	av = new_attr_value();
	av->av_value = val;
	av->av_scope = scope;

	if (save == NULL) { /* insert in front */
	    av->av_next = *av_addr;
	    *av_addr = av;
	} else { /* put after save */
	    av->av_next = save->av_next;
	    save->av_next = av;
	}
    }

    PrintObject(obj);
}

PUBLIC void
AppendAttrValue(obj, attr, val, scope)
struct object	*obj;
struct idf	*attr;
struct expr	*val;
int		scope;
/* added for efficiency, avoids a lot of CopyExpr/put_expr */
{
    struct expr *old;

    if ((old = GetAttrInScope(obj, attr, scope)) != UnKnown) {
	Append(&old->e_list, val);
    } else {
	struct expr *new = empty_list();

	Append(&new->e_list, val);
	PutAttrValue(obj, attr, new, scope);
    }
}

PRIVATE void
RemoveAvList(av)
register struct attr_value *av;
{
    register struct attr_value *next;

    while (av != NULL) {
	put_expr(av->av_value);
	next = av->av_next;
	free_attr_value(av);
	av = next;
    }
}

PUBLIC void
RemoveAttributes(obj)
struct object *obj;
{
    register short i;

    for (i = 0; i < obj->ob_attr_alloc; i++) {
	if (obj->ob_attr_array[i] != NULL) {
	    RemoveAvList(obj->ob_attr_array[i]);
	    obj->ob_attr_array[i] = NULL;
	}
    }

#if defined(__STDC__) || defined(AMOEBA)
    memset(obj->ob_attr_bits, 0, 8);
#else
    bzero(obj->ob_attr_bits, 8);
#endif

    if (obj->ob_attr_array != NULL) {
	free((char*)obj->ob_attr_array);
	obj->ob_attr_array = NULL;
	obj->ob_attr_max = -1;
	obj->ob_attr_alloc = 0;
    }
}

#define get_attr_value(result, start, scope)		\
{							\
    register struct attr_value *avp = start;		\
    register short short_scope = scope;			\
							\
    while (avp != NULL && avp->av_scope < short_scope){	\
	avp = avp->av_next;				\
    }							\
    if (avp != NULL && avp->av_scope == short_scope) {	\
	result = avp;					\
    } else { /* not found */				\
	result = NULL;					\
    }							\
}

#define AttrVal(aval) (((aval)->e_number == BUSY) ? HandleBusy(aval): (aval))

PUBLIC struct expr *
GetAttrInScope(obj, attr, scope)
struct object	*obj;
struct idf	*attr;
int scope;
{
    register int index;
    register struct attr_value *av;

    if ((index = get_attr_index(obj, attr)) >= 0) {
	get_attr_value(av, obj->ob_attr_array[index], scope);
	if (av != NULL) {
	    return(AttrVal(av->av_value));
	}
    }
    DBUG_PRINT("attr", ("%s has no attribute `%s' in scope %d",
			SystemName(obj), attr->id_text, scope));
    return(UnKnown);
}

PUBLIC struct expr *
GetAttrValue(obj, attr)
struct object	*obj;
struct idf	*attr;
{
    register int index;
    register struct attr_value *av;

    /* We want to suspend the current job if we are busy deriving the
     * attribute right now. This is accomplished by setting the value
     * of this attribute to BUSY. CurrentJob becomes ready again, when
     * the derivation finishes (succesful or not). So look for it
     * when getting the value of an already present attribute.
     */

    if ((index = get_attr_index(obj, attr)) >= 0) {
	register struct attr_value *at_first = obj->ob_attr_array[index];
	
	if (AttributeScopeNr != GLOBAL_SCOPE) {
	    /* Try non global scope first */
	    get_attr_value(av, at_first, AttributeScopeNr);
	    if (av != NULL) {
		return(AttrVal(av->av_value));
	    }
	}
	if (at_first->av_scope == GLOBAL_SCOPE) {
	    return(AttrVal(at_first->av_value));
	}
    }
    return(Derive(obj, attr));	/* possibly delivers UnKnown */
}

PUBLIC void
GlobalizeAttr(obj, attr)
struct object *obj;
struct idf    *attr;
{
    /* The current global value of attribute `attr' of `obj' isn't used,
     * so for efficiency reasons it is wise to redefine it to a current local
     * value, if there is one.
     */
    register int index;

    if ((index = get_attr_index(obj, attr)) >= 0) {
	register struct attr_value *av = obj->ob_attr_array[index];

	assert(av != NULL);
	if (av->av_scope == GLOBAL_SCOPE) {
	    av = av->av_next;
	}
	if (av == NULL) {
	    DBUG_PRINT("globalize", ("no local attribute `%s' for `%s'",
				     attr->id_text, SystemName(obj)));
	    return;
	} else {
	    register struct attr_value *prev;
	    register struct expr *value = av->av_value;

	    /* set the global value to the first local value found */
	    PutAttrValue(obj, attr, value, GLOBAL_SCOPE);

	    /* now remove all local attributes that are the same */
	    prev = obj->ob_attr_array[index];
	    /* this is the global one: it stands in front */

	    /* remove the local one we have chosen */
	    prev->av_next = av->av_next;
	    free_attr_value(av);
	    av = prev->av_next;

	    /* and remove all other local ones that are the same */
	    while (av != NULL) {
		if (is_equal(value, &av->av_value)) {
		    DBUG_PRINT("globalize", ("`%s' for `%s' from scope %d",
					     attr->id_text, SystemName(obj),
					     av->av_scope));
		    prev->av_next = av->av_next;	/* linked out */
		    free_attr_value(av);
		    av = prev->av_next;
		} else {
		    DBUG_PRINT("globalize", ("NOT `%s' for `%s' from scope %d",
					     attr->id_text, SystemName(obj),
					     av->av_scope));
		    prev = av;
		    av = av->av_next;
		}
	    }
	}
    }
}

PRIVATE void
AddAttributes(obj, attributes)
struct object *obj;
struct slist *attributes;
/* add (attr, value) pairs in `attributes' to `obj' */
{
    ITERATE(attributes, expr, spec, {
	PutAttrValue(obj, spec->e_left->e_idp, Eval(CopyExpr(spec->e_right)),
		     AttributeScopeNr);
    });
}

PUBLIC void
AddAttrList(obj, attrs)
struct expr *obj;
struct slist *attrs;
{
    if (obj->e_number == OBJECT) {
	AddAttributes(obj->e_object, attrs);
    } else {
	assert(IsList(obj));
	ITERATE(obj->e_list, expr, obj_expr, {
	    AddAttributes(obj_expr->e_object, attrs);
	});
    }
}

#define DeclAttr(var, name) DeclareAttribute(var = str2idf(name, 0))

    /* `cid' is the name of the attribute that is used to store the value-id
     * of an object between Amake runs. It is (at the moment at least) the
     * only attribute that is kept in the Statefile.
     */
    /* `id' is the name of the `current' value-id of an object, `current'
     * being the last time Amake looked at the object(-status) in the
     * filesystem.
     */


PUBLIC struct idf
    *Id_defpat,
    *Id_base,
    *Id_contents,
    *Id_computed,	/* object used as computed input */
    *Id_changed,	/* used to fake object change whle F_no_exec */
    *Id_installed,	/* boolean attribute: don't remove this object */
    *Id_cid,
    *Id_id,
    *Id_target,
    *Id_alias,
    *Id_failed,
    *Id_implicit,
    *Id_output,
    *Id_err_reported,
    *Id_dir,
    *Id_seq_nr,
    *Id_size,
    *Id_include,
    *Id_is_source;

#define BIT_SET(i, b)	(((i) & (1 << (b))) != 0)

PUBLIC void
InitAttributes()
{
    register struct idf *Id_type, *Id_cincl;
    register short i;

    for (i = 0; i < 256; i++) {
	count_bits[i] =
	    BIT_SET(i, 0) + BIT_SET(i, 1) + BIT_SET(i, 2) + BIT_SET(i, 3) +
	    BIT_SET(i, 4) + BIT_SET(i, 5) + BIT_SET(i, 6) + BIT_SET(i, 7);
    }

    DeclAttr(Id_cid,      "cid");
    DeclAttr(Id_id,       "id");
    DeclAttr(Id_alias,    "_alias");
    DeclAttr(Id_seq_nr,   "_seq_nr");
    /* used to sort generated objects according to the order of the
     * sources (used in kernel linking!)
     */

    /* `type' and `C-incl' are much-used attributes in the Amoeba conf.
     * Declaring them now makes the corresponding attribute operations
     * faster (we do a lot of them).
     */
    DeclAttr(Id_type,	  "type");
    DeclAttr(Id_cincl,	  "C-incl");

    DeclAttr(Id_output,   "_output");
    DeclAttr(Id_dir,	  "dir");	/* changes parent of object! */
    DeclAttr(Id_is_source,"is-source");

    DeclAttr(Id_implicit, "implicit");
    Id_implicit->id_flags |= F_SPECIAL_ATTR;
    DeclAttr(Id_size,	  "_size");
    DeclAttr(Id_computed, "_computed");
    DeclAttr(Id_base,     "base");
    DeclAttr(Id_defpat,   "def-pat");

    DeclAttr(Id_target,   "_target");
    DeclAttr(Id_contents, "_contents");
    DeclAttr(Id_installed,"_installed");
    DeclAttr(Id_changed,  "_changed");
    DeclAttr(Id_failed,   "_failed");
    DeclAttr(Id_include,  "included");
    Id_include->id_flags |= F_SPECIAL_ATTR;
    DeclAttr(Id_err_reported, "_err_reported");
}

/*
 * Objects
 */

PUBLIC struct object
    *RootDir, *AmakeDir, *Context, *FooObject, *IgnoreFile, *Anonymous;

PRIVATE struct object *DiagDir;
/* directory containing diagnostics and return files */

PUBLIC struct expr *
ObjectExpr(object)
struct object *object;
{
    struct expr *obj;

    if (object->ob_expr == NULL) {
	/* allocate only once */
	obj = new_expr(OBJECT);
	obj->e_type = T_OBJECT;
	obj->e_object = object;
	object->ob_expr = obj;
    }
    return(object->ob_expr);
}

PRIVATE struct object *
CreateObject(idp, parent)
struct idf *idp;
struct object *parent;
{
    static int last_object_index = 0;
    register struct object *obj;

    idp->id_kind |= I_OBJECT;
    obj = new_object();
    obj->ob_idp = idp;
    obj->ob_parent = parent;
    obj->ob_index = last_object_index++;
    obj->ob_attr_max = -1;	/* no attributes allocated yet */
    obj->ob_attr_alloc = 0;	/* no attributes allocated yet */

    /* link objects with same last component */
    obj->ob_same_comp = idp->id_object;
    idp->id_object = obj;
    
    /* furthermore all objects are linked together */
    obj->ob_next = FirstObject;
    FirstObject = obj;

    return(obj);
}

PRIVATE struct slist *DiagList = NULL;

PUBLIC struct object *
NewDiagFile()
{
    static char diag[10];
    static int  last_diag = 0;
    struct object *obj;

    if (IsEmpty(DiagList)) {
	/* Create the temporary in the diagnostics directory rather than
	 * in the .Amake directory. File creation in a directory containing
	 * already hundreds of entries is typically very slow.
	 */
	if (last_diag == 0) {
	    MakePrivateDir(DiagDir);
	}
	(void)sprintf(diag, "diag_%04d", last_diag++);
	obj = CreateObject(str2idf(diag, 1), DiagDir);
	obj->ob_flags = O_DIAG | O_TMP;
	DBUG_PRINT("diagobj", ("new diag `%s'", SystemName(obj)));
    } else {
	obj = RemoveHead(&DiagList, object);
	DBUG_PRINT("diagobj", ("return diag `%s'", SystemName(obj)));
    }
    return(obj);
}

PUBLIC void
PutDiagFile(obj)
struct object *obj;
/* Diagnostic file is not needed anymore; put it on the DiagList so that it
 * can be used by commands still to be done.
 */
{
    DBUG_PRINT("diagobj", ("put diag `%s'", SystemName(obj)));
    Append(&DiagList, obj);
}

PUBLIC void
RemoveDiagFiles()
{
    ITERATE(DiagList, object, obj, {
	DoDelete(SystemName(obj), NON_FATAL);
    });
}

PUBLIC struct object *
NewReturnFile()
{
    static char ret[10];
    static int  last_ret = 0;
    struct object *obj;

    if (last_ret == 0) {
	MakePrivateDir(DiagDir);
    }
    (void)sprintf(ret, "ret_%04d", last_ret++);
    obj = CreateObject(str2idf(ret, 1), DiagDir);
    obj->ob_flags = O_RETURN | O_TMP;
    return(obj);
}

PRIVATE struct slist *VariantList = NULL;

PUBLIC struct object *
Variant(obj)
struct object *obj;
/* Deliver an object under which may be stored safely, without affecting
 * versions created with other invocations.
 * I.e., a variant of "file.o" could be ".Amake/version00/file.o", if
 * version00 didn't contain "file.o" yet.
 * This function also creates the directory .Amake/versionXX when necessary.
 */
{
    register int i;

    ITERATE(VariantList, object, dir, {
	register struct object *same;

	/* see if this variant is still available */
	for (same = obj->ob_idp->id_object;
	     same != NULL && same->ob_parent != dir;
	     same = same->ob_same_comp)
	    ;
	if (same == NULL) { /* OK: this variant is not used yet */
	    return(ObjectWithinParent(obj->ob_idp, dir));
	}
    });

    /* we'll have to create a new variant directory, look for a new one */
    for (i = 0 ;; i++) {
	register struct idf *var_idp;
	register struct object *variant_dir;
	char version[12];

	(void)sprintf(version, "version%02d", i);
	var_idp = str2idf(version, 1);
	if (var_idp->id_object == NULL) { /* not used yet */
	    DBUG_PRINT("version", ("new version directory: %s", version));
	    variant_dir = ObjectWithinParent(var_idp, AmakeDir);
	    Append(&VariantList, variant_dir);
	    MakePrivateDir(variant_dir);
	    return(ObjectWithinParent(obj->ob_idp, variant_dir));
	}
    }
}

PRIVATE void
InsertVariant(varlist, variant)
struct slist **varlist;
struct object *variant;
{
    ITERATE(*varlist, object, obj, {
	if (strcmp(variant->ob_idp->id_text, obj->ob_idp->id_text) < 0) {
	    Insert(varlist, cons, variant);
	    return;
	}
    });
    Append(varlist, variant);
}

PUBLIC void
InitVariants()
/* Initialise the list Variants containing the variant directories that are
 * used. This should be done after the Statefile has been read, because
 * at that moment the used variant directories are known.
 */
{
    register struct object *obj;

    for (obj = FirstObject; obj != NULL; obj = obj->ob_next) {
	/* variant directories reside in AmakeDir and start with "version" */
	if (obj->ob_parent == AmakeDir &&
	    strncmp(obj->ob_idp->id_text, "version", 7) == 0) {
	    /* insert it in VariantList, in alphabetic order */
	    InsertVariant(&VariantList, obj);
	}
    }
}

#ifdef amunix
#define ROOT_NAME 	"/"
#define CTXT_NAME	"."
#define PATH_SEP_S	"/"
#define PATH_SEP_C	'/'
#endif

PUBLIC void
object_init()
/*
 * Initilize a few standard objects, for now: RootDir, Context.
 *
 * At this moment RootDir is a child of Context, for convenience. To avoid
 * multiple objects representing the same file, a 'pwd' like routine
 * should be included resulting in a object tree structure representing
 * the actual file system structure.
 */
{
    struct object *dev;

    InitAttributes();
    Context = CreateObject(str2idf(CTXT_NAME, 0), (struct object *)NULL);
    Context->ob_parent = Context;	
    /* Context is its own parent, to prevent pointer problems */
    RootDir = CreateObject(str2idf("/",0), Context);
    FooObject = CreateObject(str2idf("*foo*",0), Context);
    Anonymous = CreateObject(str2idf("*anonymous*", 0), Context);
#ifdef amunix
    dev = CreateObject(str2idf("dev",0), RootDir);
    IgnoreFile = CreateObject(str2idf("null",0), dev);
#endif
    AmakeDir = CreateObject(str2idf(".Amake",0), Context);
    DiagDir = CreateObject(str2idf("Diag", 0), AmakeDir);
}


PUBLIC struct object *
ObjectWithinParent(comp, parent)
struct idf *comp;
struct object *parent;
{
    register struct object *obj;

    /* maybe it already exists; loop over the objects having `comp' as last
     * component
     */
    for (obj = comp->id_object; obj != NULL; obj = obj->ob_same_comp) {
	if (obj->ob_parent == parent) {
	    return(obj);
	}
    }

    /* not found, make new one */
    return(CreateObject(comp, parent));
}

#define MAX_NAM_LEN	256
PRIVATE char obj_name[MAX_NAM_LEN];

PRIVATE void
add_object_name(obj)
struct object *obj;
{
    if (obj->ob_parent == Context) {
        if (obj == RootDir) {
	    (void)strcat(obj_name, "/");
        } else {
	    /* "./" not needed */
	    (void)strcat(obj_name, QuotedId(obj->ob_idp->id_text));
	}
    } else {
	if (obj->ob_parent != RootDir) /* avoid "//" */	{
	    add_object_name(obj->ob_parent);
	}
	(void)strcat(obj_name, "/");
        (void)strcat(obj_name, QuotedId(obj->ob_idp->id_text));
    }
}
    
PUBLIC char *
ObjectName(obj)
struct object *obj;
{
    obj_name[0] = 0;
    add_object_name(obj);
    return(&obj_name[0]);
}

#define EOS '\0'


PRIVATE void
add_system_name(obj)
struct object *obj;
{
    /* Almost the same as add_object_name, only the component names
     * are never quoted.
     */
    if (obj->ob_parent == Context) {
        if (obj == RootDir) {
	    (void)strcat(obj_name, ROOT_NAME);
        } else {
	    /* "./" not needed */
	    (void)strcat(obj_name, obj->ob_idp->id_text);
	}
    } else {
	if (obj->ob_parent != RootDir) /* avoid "//" */	{
	    add_system_name(obj->ob_parent);
	}
	(void)strcat(obj_name, PATH_SEP_S);
        (void)strcat(obj_name, obj->ob_idp->id_text);
    }
}
    
PUBLIC char *
SystemName(obj)
struct object *obj;
{
    obj_name[0] = 0;
    add_system_name(obj);
    return(&obj_name[0]);
}

#ifdef amunix

#ifdef USE_RSH

#if !defined(__STDC__) && !defined(AMOEBA)
#  include <sys/param.h>
#else
#  define MAXPATHLEN      1024
#endif

#ifndef CWD
#define CWD "CWD" /* env. var. containing current working directory */
#endif

PUBLIC char *
WorkDir()
{
    static char WorkingDirectory[MAXPATHLEN], *WorkingDir;
    static int WorkingDirDone = FALSE;

    if (!WorkingDirDone) {
	extern char *getenv();
	char *cwd;

	WorkingDirDone = TRUE;
	if ((cwd = getenv(CWD)) != NULL) { /* use CWD if available */
	    (void)strcpy(WorkingDirectory, cwd);
	} else {
#ifdef HAS_GETWD
	    extern char *getwd();

	    if (getwd(WorkingDirectory) == NULL) {
		Warning("couldn't find working directory");
		return(WorkingDir = NULL);
	    }
#else
	    Warning("no environment variable $CWD");
	    return(WorkingDir = NULL);
#endif
	}
	WorkingDir = WorkingDirectory;
    }
    return(WorkingDir);
}

#endif

PRIVATE struct object *
SystemFirstComponent(s)
char *s;
{
    if (*s == EOS) {	/* i.e. we have "/foo/bar", with '/' replaced by EOS */
	return(RootDir);
    } else {
	return(ObjectWithinParent(str2idf(s, 1), Context));
    }
}

#endif /* amunix */

PUBLIC struct object *
SystemNameToObject(s)
char *s;
{
    struct object *obj;
    char *p;

    if ((p = strchr(s, PATH_SEP_C)) != NULL) {
	*p = EOS;		/* avoid copying components */
	obj = SystemFirstComponent(s);
	*p = PATH_SEP_C;	/* restore */
	s = p + 1;

	while ((p = strchr(s, PATH_SEP_C)) != NULL) {
	    *p = EOS;
	    obj = ObjectWithinParent(str2idf(s, 1), obj);
	    *p = PATH_SEP_C;
	    s = p + 1;
	}
    } else {
	/* no seperator: object must be in Context */
	obj = Context;
    }

    /* handle last component */
    if (*s != EOS) {
	return(ObjectWithinParent(str2idf(s, 1), obj));
    } else {
	return(obj);
    }
}

