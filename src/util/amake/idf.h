/*	@(#)idf.h	1.2	94/04/07 14:51:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * The following namespaces exist:
 * - Cluster names			from cluster definitions
 * - Tool names				from tooldefinitions
 * - Parameter names (options & io)	from the tool/function header
 * - variable names			from variable def
 * - function names			a few builtin functions
 * - Object names			computed by amake ('/' and '&' oper)
 * - Attribute names			directly attached/declared or derived
 *
 * We have only one namelist, so we will have to share it.
 * But don't allow two of {Cluster, Tool, Macro, Function} have the same name
 */

#define I_PARAM		0x001
#define I_OBJECT	0x002
#define I_ATTRIBUTE	0x004
#define I_CLUSTER	0x008
#define I_TOOL		0x010
#define I_VARIABLE	0x020
#define I_FUNCTION	0x040
#define I_ERROR		0x080
#define I_GENERIC	0x100

#define I_CALL		(I_TOOL|I_FUNCTION)
#define I_SHARED	(I_TOOL|I_FUNCTION|I_GENERIC)

struct id_struct {
    short	       id__token;	/* ID or one of the keywords */
    short	       id__kind;	/* bitset of above possibilities */
    int		       id__flags;
    struct param      *id__param;
    struct object     *id__object;	/* all having id_text as last part */
    struct attr_descr *id__attr_descr;
    struct variable   *id__variable;
    struct cluster    *id__cluster;
    union {
        struct tool	*id__tool;
	struct function *id__function;
	struct Generic  *id__generic;
    } id_misc;
};

/*
 * Instantiation of the generic namelist module.
 */

#define IDF_TYPE	struct id_struct
#define IDF_NAME	id__struct
#define IDF_HSIZE	12
/*
 * Only use the first IDF_HSIZE characters for hashing (although names
 * may have any length).
 */

#define id_token	id__struct.id__token
#define id_kind		id__struct.id__kind
#define id_flags	id__struct.id__flags
#define id_param	id__struct.id__param
#define id_value	id__struct.id__value
#define id_object	id__struct.id__object
#define id_attr_descr	id__struct.id__attr_descr
#define id_variable	id__struct.id__variable
#define id_cluster	id__struct.id__cluster

#define id_tool		id__struct.id_misc.id__tool
#define id_function	id__struct.id_misc.id__function
#define id_generic	id__struct.id_misc.id__generic

#define F_IDENT		0x01 /* quoted keywords are normal identifiers */
#define F_NO_EVAL	0x02 /* internal variables are not to be evaluated */
#define F_CALLED        0x04
#define F_ERROR_ID      0x08 /* generated error identifier */
#define F_SPECIAL_ATTR  0x10 /* attribute with special semantics */
#define F_SPECIAL_FUNC	0x20 /* doesn't first evaluate its args */
#define F_IO_ATTR	0x40 /* attribute name used in tool IO */
#define F_QUIET		0x80 /* don't complain if this variable is redefined */

#include <idf_pkg.spec>


