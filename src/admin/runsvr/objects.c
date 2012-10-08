/*	@(#)objects.c	1.6	96/02/27 10:18:04 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define _POSIX_SOURCE
#include <limits.h>
#include <ctype.h>

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "ampolicy.h"
#include "module/name.h"
#include "module/stdcmd.h"
#include "module/prv.h"
#include "module/mutex.h"
#include "module/buffers.h"
#include "module/ar.h"
#include "module/rnd.h"

#include "runsvr.h"
#include "runrights.h"
#include "pool_dir.h"
#include "findbest.h"
#include "objects.h"

extern char *strdup();

/* runsvr state file magic string: */
#define RUN_MAGIC_ASCII	"#runadmin#"

/*
 * Special attributes used in (ASCII) state file.
 */
#define ATTR_GETPORT	"getport"	/* idem dito */
#define ATTR_DIRCAP	"dircap"	/* attribute of pooldir object */
#define ATTR_CHECK	"check"		/* attribute of any object */

#define ADMIN_SUFFIX	".adm"
#define TMP_SUFFIX	".tmp"

#define MAX_NUM_OBJECTS	100    /* should be enough */

static mutex Obj_array_mutex;  /* must be acquired before changing Obj_array */
static object Obj_array[MAX_NUM_OBJECTS];

static capability Null_cap;

port Priv_port;
static port Pub_port;

#define BAD_OBJNUM(num)	((num) < 0 || (num) >= MAX_NUM_OBJECTS)
#define BAD_OBJTYPE(t)	((t) != OBJ_SUPER && (t) != OBJ_POOL)


#include "capset.h"
#include "soap/soap.h"

/*
 * Chmod a file to ff:0:0.  Return 1 on success, 0 on failure.
 */
static int
secure_file(f)
char *	f;
{
    static long	cols[] = { 0xff, 0, 0 };
    capset	dir;
    capset *	dirp;
    char *	name;
    errstat	err;

    name = strrchr(f, '/');
    if (name == (char *) 0) {
	name = f;
	dirp = SP_DEFAULT;
    } else {
	*name++ = '\0';
	if (*name == '\0') {
	    fprintf(stderr, "secure_file: filename was zero length\n");
	    return 0;
	}
	if ((err = sp_lookup(SP_DEFAULT, f, &dir)) != STD_OK) {
	    fprintf(stderr, "secure_file: lookup of %s failed (%s)\n",
							    f, err_why(err));
	    return 0;
	}
	dirp = &dir;
    }
    if ((err = sp_chmod(dirp, name, 3, cols)) != STD_OK) {
	fprintf(stderr, "secure_file: chmod of %s failed (%s)\n",
							    f, err_why(err));
	return 0;
    }
    return 1;
}


void
obj_init()
{
    /*
     * Initalisation of the object module.
     * It marks all objects as not in use, and initialises the mutexes.
     */
    objnum obj_nr;

    mu_init(&Obj_array_mutex);
    for (obj_nr = 0; obj_nr < MAX_NUM_OBJECTS; obj_nr++) {
	object *obj = &Obj_array[obj_nr];

	mu_init(&obj->obj_mu);
	obj->obj_in_use = 0;
    }
}

static void
init_pool_obj(obj)
object *obj;
{
    /*
     * Try to initialise a OBJ_POOL object.  This can fail because the pooldir
     * may have been destroyed in the meantime.  In that case we'll retry
     * when someone asks for this pool object.  Otherwise it will be garbage
     * collected after a while.
     */
    struct pooldir *pool;
    attribute *at;
    errstat err;

    if ((err = add_pooldir(&obj->obj_pooldir, &pool)) != STD_OK) {
	/* Non fatal.  Also don't throw it out; maybe Soap is down. */
	fprintf(stderr, "could not initialise pooldir!! (%s)\n", err_why(err));
	obj->obj_pool = NULL;
	return;
    }

    obj->obj_pool = pool;

    /* initialise pool parameters that were saved in the state file */
    for (at = obj->obj_attrlist; at != NULL; at = at->attr_next) {
	if (isupper(*at->attr_name)) {
	    /* Attribute names starting with uppercase letter
	     * are assumed to be pool parameters.
	     * 
	     * Ignore the error code;  maybe the parameter is temporarily
	     * inapplicable because of a pool reconfiguration
	     * (set_pool_param will complain on standard error in this case).
	     */
	    (void) set_pool_param(pool, at->attr_name, at->attr_value);
	}
    }
}

errstat
obj_find(prv, allowed, required, objp)
private *prv;		/* from the RPC request */
int allowed;		/* object types allowed for operation requested */
rights_bits required;	/* at least these rights must be present */
object **objp;		/* the object, if this function returns STD_OK */
{
    /*
     * Try to find & validate an object indicated by a private part.
     * The parameters also specify allowed object types (super or pool)
     * and rights required.
     */
    objnum num;
    object *obj;
    rights_bits r;
    
    num = prv_number(prv);
    if (BAD_OBJNUM(num)) {	/* range check */
	return STD_CAPBAD;
    }

    obj = &Obj_array[num];
    mu_lock(&obj->obj_mu);

    if (!obj->obj_in_use ||				/* validity check */
	(prv_decode(prv, &r, &obj->obj_check) < 0) ||	/* random check */
	(obj->obj_type & allowed) == 0)			/* type check */
    {
	mu_unlock(&obj->obj_mu);
	return STD_CAPBAD;
    }

    if ((r & required) != required) {	/* rights check */
	mu_unlock(&obj->obj_mu);
	return STD_DENIED;
    }

    if (obj->obj_type == OBJ_POOL && obj->obj_pool == NULL) {
	/* pool not yet initialised */
	init_pool_obj(obj);
	if (obj->obj_pool == NULL) {	/* Soap unavailable? */
	    mu_unlock(&obj->obj_mu);
	    return STD_NOTNOW;
	}
    }

    *objp = obj;
    return STD_OK;
}

void
obj_disallow_mods()
{
    mu_lock(&Obj_array_mutex);
}

void
obj_allow_mods()
{
    mu_unlock(&Obj_array_mutex);
}

void
obj_delete(obj)
object **obj;
{
    /*
     * Delete locked object obj.
     * The Obj_array_mutex must have been acquired already.
     */
    object *objp = *obj;
    attribute *attr, *next;

    unpublish_pooldir(objp - Obj_array);
    objp->obj_in_use = 0;

    /* free the attribute list */
    for (attr = objp->obj_attrlist; attr != NULL; attr = next) {
	next = attr->attr_next;
	free((_VOIDSTAR) attr->attr_name);
	free((_VOIDSTAR) attr->attr_value);
	free((_VOIDSTAR) attr);
	attr = next;
    }
    objp->obj_attrlist = NULL;
    
    mu_unlock(&objp->obj_mu);
    *obj = NULL;	/* invalidate object reference */
}

static void
make_pub_cap(putcap, num, rights, check)
capability *putcap;	/* out: capability that can be published */
objnum num;		/* number of the object */
rights_bits rights;	/* its rights */
port *check;		/* its random check field */
{
    if (prv_encode(&putcap->cap_priv, num, rights, check) != 0) {
	/* Only fails when too many rights were requested,
	 * and we know that's not the case.
	 */
	fatal("prv_encode failure (cannot create put cap)");
    }

    putcap->cap_port = Pub_port;
}

static void
init_object(obj_nr)
objnum obj_nr;
{
    static port null_port;
    struct object *obj = &Obj_array[obj_nr];

    obj->obj_type = (obj_nr == 0) ? OBJ_SUPER : OBJ_POOL;
    obj->obj_check = null_port;
    obj->obj_timeleft = OBJ_FRESH;
    obj->obj_attrlist = NULL;
    obj->obj_pooldir = Null_cap;
    obj->obj_pool = NULL;
    obj->obj_in_use = 1;
}

errstat
obj_new(objcap, type, pool)
capability *objcap;	/* out: capability for the new object */
int type;		/* type of the new object */
struct pooldir *pool;	/* if type == OBJ_POOL, the corresponding pooldir */
{
    errstat err = STD_OK;
    object *obj;
    objnum obj_nr;

    obj_disallow_mods();

    /* find unused object number */
    for (obj_nr = 0; obj_nr < MAX_NUM_OBJECTS; obj_nr++) {
	if (!Obj_array[obj_nr].obj_in_use) {
	    break;
	}
    }

    if (obj_nr >= MAX_NUM_OBJECTS) {
	obj_allow_mods();
	return STD_NOSPACE;
    }

    obj = &Obj_array[obj_nr];
    mu_lock(&obj->obj_mu);

    init_object(obj_nr);
    obj->obj_type = type;
    uniqport(&obj->obj_check);

    err = obj_set_attribute(obj, ATTR_CHECK, ar_port(&obj->obj_check));

    if (err == STD_OK && type == OBJ_POOL) {
	obj->obj_pool = pool;
	if (pool != NULL) {
	    obj->obj_pooldir = pool->pd_dir;
	    err = obj_set_attribute(obj, ATTR_DIRCAP,
				    ar_cap(&obj->obj_pooldir));
	}
	publish_pooldir(&obj->obj_pooldir, obj_nr);
    }

    if (err == STD_OK) {
	make_pub_cap(objcap, obj_nr, RUN_RGT_ALL, &obj->obj_check);
	mu_unlock(&obj->obj_mu);
    } else {
	obj_delete(&obj);
    }

    obj_allow_mods();

    return err;
}

void
obj_release(obj)
object *obj;
{
    obj->obj_timeleft = OBJ_FRESH;
    mu_unlock(&obj->obj_mu);
}

/*
 * Format of the state file is as follows:
 *
 * --------------------------------------------
 * #runadmin#
 * 0: getport  = 0:1:2:3:4:5			; port to listen to
 * 0: check    = 1:2:3:4:5			; check field super cap
 *
 * 1: dircap   = 2:3:4:5:6:7/88(ff)/8:9:a:b:c:d	; pool dir corr. to run obj 1
 * 1: check    = 4:5:6:7:8:9			; its check field
 * 1: CACHEMEM = 1234				; optional pool params
 * --------------------------------------------
 * ^    ^           ^- attribute value
 * |    +---------- attribute name (Upper case indicates pool param)
 * +--------------- object number (0 is super object)
 */

/* Upon failure, don't forget to perform cleanup actions (at Exit) */
#define Fail(list)	{ error list; retval = 0; goto Exit; }

int
obj_store(dir, name)
char *dir;
char *name;
{
    /* After (re-)initialisation this function has to be called
     * to write the current object structure to disk,
     * so that we can recover from it next time.
     */
    int		retval = 1;	/* indicating success */
    char	tempname[PATH_MAX];
    char	statename[PATH_MAX];
    FILE       *fp;
    objnum	obj_nr;

    obj_disallow_mods();	/* no changes allowed while we're saving */

    if (strlen(dir) + strlen(name) > PATH_MAX - 10) {
	Fail(("filename \"%s/%s\" too big", dir, name));
    }

    (void) sprintf(tempname, "%s/%s%s", dir, name, TMP_SUFFIX);
    (void) sprintf(statename, "%s/%s%s", dir, name, ADMIN_SUFFIX);
    if ((fp = fopen(tempname, "w")) == NULL) {
	Fail(("could not open temp file \"%s\"", tempname));
    }

    /* magic string */
    fprintf(fp, "%s\n", RUN_MAGIC_ASCII);

    /*
     * Write the attributes of all objects marked in-use
     */
    for (obj_nr = 0; obj_nr < MAX_NUM_OBJECTS; obj_nr++) {
	object *obj = &Obj_array[obj_nr];

	/* The obj_in_use field is only changed while the Obj_array_mutex
	 * is locked, so we don't have to lock every object before
	 * examining it.
	 */
	if (obj->obj_in_use) {
	    attribute *at;

	    mu_lock(&obj->obj_mu);
	    for (at = obj->obj_attrlist; at != NULL; at = at->attr_next) {
		fprintf(fp, "%ld: %s = %s\n",
			obj_nr, at->attr_name, at->attr_value);
	    }
	    fprintf(fp, "\n");
	    mu_unlock(&obj->obj_mu);
	}
    }

    /* Install the new state file. */
    if (fclose(fp) != 0) {
	Fail(("error while closing file %s (%s)", tempname, strerror(errno)));
    }

#if 0
    if (sync() != 0) {
	/* TODO: sync() should do a b_sync(), but is currently a no-op */
	Fail(("could not sync file `%s' to disk", tempname));
    }
#endif

    if (rename(tempname, statename) != 0) {
	Fail(("cannot rename \"%s\" to \"%s\"", tempname, statename));
    }

    if (!secure_file(statename))
	Fail(("cannot chmod \"%s\" (%s)\n", statename));
    
Exit:
    obj_allow_mods();

    return retval;
}

errstat
new_super_obj(supercap)
capability *supercap;
{
    /*
     * Start fresh; think up a new get port, and create corresponding put port.
     * Also add it as "getport" attribute to a newly created super object,
     * so that it gets stored in the statefile.
     */
    errstat err;

    uniqport(&Priv_port);
    priv2pub(&Priv_port, &Pub_port);

    err = obj_new(supercap, OBJ_SUPER, (struct pooldir *)NULL);
    if (err != STD_OK) {
	return err;
    }

    err = obj_set_attribute(&Obj_array[0], ATTR_GETPORT, ar_port(&Priv_port));
    if (err != STD_OK) {
	fprintf(stderr, "could not set `%s' attribute of super object (%s)",
		ATTR_GETPORT, err_why(err));
    }
    
    return err;
}


#define MAX_LINESIZE 80

static int line_number = 0;

static char *
skipspace(str)
char *str;
{
    while (*str == ' ' || *str == '\t') {
	str++;
    }

    return str;
}

static char *
save_string(str, len)
char *str;
size_t len;
{
    char *mem;

    if ((mem = (char *)malloc(len + 1)) == NULL) {
	fatal("out of memory while reading state file");
    }
    strncpy(mem, str, len);
    mem[len] = '\0';
    return mem;
}

static int
parse_assignment(fp, ret_number, ret_attr, ret_value)
FILE   *fp;
objnum *ret_number;
char  **ret_attr;
char  **ret_value;
{
    /*
     * get an assignment of the form "objnum: attr = val" from file fp.
     * returns 1 on success, 0 on EOF, -1 on syntax error.
     */
    char   *err_string = "";
#   define SyntaxError(err)	{ err_string = err; goto syntax; }

    char    line[MAX_LINESIZE];
    objnum  number;
    char   *attr = NULL;
    char   *value = NULL;
    char   *pos = NULL, *start;

    /*
     * get non empty line, checking for overflow
     */
    for (;;) {
	size_t length;

	if (fgets(line, MAX_LINESIZE, fp) == NULL) {
	    return 0;	/* end of file */
	}
	line_number++;

	length = strlen(line);
	if (length >= MAX_LINESIZE - 2) {
	    SyntaxError("too long");
	}
	if (length == 0 || line[length - 1] != '\n') {
	    SyntaxError("not terminated with newline");
	}

	start = skipspace(line);
	if (*start != '\n') {
	    /* found a non empty line */
	    break;
	}
    }

    /*
     * get object number
     */
    number = strtol(line, &pos, 0);
    if (pos == NULL) {
	SyntaxError("missing object number");
    }

    pos = skipspace(pos);
    if (*pos != ':') {
	SyntaxError("mssing character ':'");
    }
    pos = skipspace(pos + 1);

    /*
     * get attribute, which may consist of upper- and lowercase letters,
     * digits and '-' (which is used in parameters such as "PREF-mc68000")
     */
    start = pos;
    while (isupper(*pos) || islower(*pos) || isdigit(*pos) || *pos == '-') {
	pos++;
    }
    if ((pos - start) == 0) {
	SyntaxError("missing attribute");
    }
    attr = save_string(start, (size_t)(pos - start));

    pos = skipspace(pos);
    if (*pos != '=') {
	SyntaxError("missing character '='");
    }
    pos = skipspace(pos + 1);

    /*
     * get value
     */
    start = pos;
    while (!isspace(*pos)) {
	/* value is string from start to first space (normally '\n') */
	pos++;
    }
    if ((pos - start) == 0) {
	SyntaxError("missing value");
    }
    value = save_string(start, (size_t)(pos - start));

    /*
     * check for remaining garbage
     */
    pos = skipspace(pos);
    if (*pos != '\n') {
	SyntaxError("extra characters following assignment");
    }

    /* success */
    *ret_number = number;
    *ret_attr = attr;
    *ret_value = value;
    return 1;

syntax:
    fprintf(stderr, "syntax error, line %d: %s\n", line_number, err_string);
    return -1;
}


errstat
obj_set_attribute(obj, name, value)
object *obj;
char   *name;
char   *value;
{
    attribute *attr;
    char *copy_name;
    char *copy_value;

    copy_name = strdup(name);
    copy_value = strdup(value);
    if (copy_name == NULL || copy_value == NULL) {
	return STD_NOSPACE;
    }

    /*
     * maybe this attribute is already on the list
     */
    for (attr = obj->obj_attrlist; attr != NULL; attr = attr->attr_next) {
	if (strcmp(attr->attr_name, name) == 0) {
	    break;
	}
    }

    if (attr != NULL) {
	/* update existing attribute */
	free((_VOIDSTAR) attr->attr_value);
	free((_VOIDSTAR) attr->attr_name);
    } else {
	/* add new attribute */
	if ((attr = (attribute *)malloc(sizeof(attribute))) == NULL) {
	    return STD_NOSPACE;
	}
	attr->attr_next = obj->obj_attrlist;
	obj->obj_attrlist = attr;
    }

    attr->attr_name = copy_name;
    attr->attr_value = copy_value;
    return STD_OK;
}

static char *
obj_get_attribute(obj, name)
object *obj;
char   *name;
{
    attribute *attr;

    for (attr = obj->obj_attrlist; attr != NULL; attr = attr->attr_next) {
	if (strcmp(attr->attr_name, name) == 0) {
	    return attr->attr_value;
	}
    }

    /* not found */
    return NULL;
}

static int
attr_to_port(obj_nr, attr, portp)
objnum obj_nr;
char *attr;
port *portp;
{
    object *obj = &Obj_array[obj_nr];
    port attr_port;
    char *value;
    char *tail;
	    
    if ((value = obj_get_attribute(obj, attr)) == NULL) {
	fprintf(stderr, "object %ld: attribute `%s' missing\n", obj_nr, attr);
	return 0;
    }

    tail = ar_toport(value, &attr_port);
    if (tail == NULL || *tail != '\0') {
	fprintf(stderr, "object %ld: bad port for attr `%s'", obj_nr, attr);
	return 0;
    }

    *portp = attr_port;
    return 1;
}

static int
attr_to_cap(obj_nr, attr, cap)
objnum obj_nr;
char *attr;
capability *cap;
{
    object *obj = &Obj_array[obj_nr];
    capability attr_cap;
    char *value;
    char *tail;
	    
    if ((value = obj_get_attribute(obj, attr)) == NULL) {
	fprintf(stderr, "object %ld: attribute `%s' missing\n", obj_nr, attr);
	return 0;
    }

    tail = ar_tocap(value, &attr_cap);
    if (tail == NULL || *tail != '\0') {
	fprintf(stderr, "object %ld: bad capability for attr `%s'",
		obj_nr, attr);
	return 0;
    }

    *cap = attr_cap;
    return 1;
}

int
obj_read(dir, name, supercap)
char *dir;
char *name;
capability *supercap;	/* is checked with super check field in state file  */
{
    /*
     * read previously saved configuration from state file "dir/name.adm".
     */
    int		retval = 1, status;
    char	statename[PATH_MAX];
    char	line[MAX_LINESIZE];
    objnum	obj_nr;
    rights_bits r;
    char       *attr;
    char       *value;
    char       *newline;
    FILE       *fp = NULL;
    errstat	err;
    
    obj_disallow_mods();

    /*
     * open state file
     */
    if (strlen(dir) + strlen(name) > PATH_MAX - 10) {
	Fail(("filename \"%s/%s\" too big", dir, name));
    }
    (void) sprintf(statename, "%s/%s%s", dir, name, ADMIN_SUFFIX);
    if ((fp = fopen(statename, "r")) == NULL) {
	Fail(("could not open admin file \"%s\"", statename));
    }

    /*
     * get and check magic string
     */
    if (fgets(line, MAX_LINESIZE, fp) == NULL) {
	Fail(("could not get first line from \"%s\"", statename));
    }
    if ((newline = strchr(line, '\n')) == NULL) {
	Fail(("%s: first line not properly terminated", statename));
    }
    *newline = '\0';

    if (strcmp(line, RUN_MAGIC_ASCII) != 0) {
	Fail(("%s: bad magic number", statename));
    }

    /*
     * Read and interpret assignments.
     * They are of the form
     *		objnum : attrname = value
     *
     * A "getport" entry for object 0 must be present
     */
    line_number = 1;
    while ((status = parse_assignment(fp, &obj_nr, &attr, &value)) == 1) {
	object *obj;

	if (BAD_OBJNUM(obj_nr)) {
	    Fail(("%s: bad object number %ld", statename, obj_nr));
	}

	obj = &Obj_array[obj_nr];
	if (!obj->obj_in_use) {
	    init_object(obj_nr);
	}

	/* 
	 * Add it to the obj_attrlist of the object.
	 * Attributes starting with a lowercase letter are for internal
	 * use, and used to initialise the objects below.
	 * Otherwise we assume it's a pooldir parameter.
	 * As soon as the pool can be initialised, the parameters will
	 * be set accordingly.
	 */
	if ((err = obj_set_attribute(obj, attr, value)) != STD_OK) {
	    Fail(("could not set attribute `%s' (%s)", attr, err_why(err)));
	}

	free((_VOIDSTAR) attr);
	free((_VOIDSTAR) value);
    }

    if (status == -1) {
	Fail(("%s: unrecoverable error", statename));
    }

    /*
     * Initialise Priv_port from "getport" attribute of super object.
     */
    if (!attr_to_port((objnum)0, ATTR_GETPORT, &Priv_port)) {
	Fail(("no `%s' attribute in file `%s'", ATTR_GETPORT, statename));
    }
    priv2pub(&Priv_port, &Pub_port);

    /*
     * Initialise check field of super object.  It must be present.
     * Also check it against the super cap that was published.
     */
    if (!attr_to_port((objnum)0, ATTR_CHECK, &Obj_array[0].obj_check)) {
	Fail(("no check field for super object in file `%s'", statename));
    }
    if (prv_decode(&supercap->cap_priv, &r, &Obj_array[0].obj_check) < 0) {
	Fail(("%s: `%s' of super object inconstent with super cap",
	      statename, ATTR_CHECK));
    }

    /*
     * Initialise "check" and "dircap" attributes for pool objects.
     * For now we don't take any action (apart from complaining)
     * when one of them is missing.
     * Maybe someone has edited the file by hand, and made a mistake.
     */
    for (obj_nr = 0; obj_nr < MAX_NUM_OBJECTS; obj_nr++) {
	object *obj = &Obj_array[obj_nr];
	int bad = 0;

	if (obj->obj_in_use && obj->obj_type == OBJ_POOL) {
	    if (!attr_to_port(obj_nr, ATTR_CHECK, &obj->obj_check)) {
		bad++;
	    }
	    if (!attr_to_cap(obj_nr, ATTR_DIRCAP, &obj->obj_pooldir)) {
		bad++;
	    }
	    if (bad) {
		fprintf(stderr, "fix file `%s' manually!!\n", statename);
	    } else {
		/* Now scan the pool.  We do this for all known pools
		 * during initialisation to make sure we are using the
		 * host capabilities with as many rights as possible.
		 * This is required for host cap switching.
		 */
		init_pool_obj(obj);
	    }
	}
    }

Exit:
    if (fp != NULL) {
	fclose(fp);
    }

    obj_allow_mods();

    return retval;
}

errstat
impl_std_age(h)
header *h;
{
    object  *super_obj;
    errstat  err;
    objnum   obj_nr;
    int	     timed_out = 0;
	
    err = obj_find(&h->h_priv, OBJ_SUPER, RUN_RGT_ADMIN, &super_obj);
    if (err != STD_OK) {
	return err;
    }
    obj_release(super_obj);

    obj_disallow_mods();

    /*
     * decrement the timeleft field of all objects
     */
    for (obj_nr = 0; obj_nr < MAX_NUM_OBJECTS; obj_nr++) {
	object *obj = &Obj_array[obj_nr];

	if (obj->obj_in_use && obj->obj_type == OBJ_POOL) {
	    /* only age pool objects */
	    mu_lock(&obj->obj_mu);

	    obj->obj_timeleft--;
	    if (debugging) {
		printf("time left for pool object %ld is %d\n",
		       obj_nr, obj->obj_timeleft);
	    }
	    if (obj->obj_timeleft < 0) {
		timed_out++;
		if (obj->obj_pool != NULL) {
		    del_pooldir(obj->obj_pool);
		    obj->obj_pool = NULL;
		}
		obj_delete(&obj);	/* with implicit unlock */
	    } else {
		mu_unlock(&obj->obj_mu);
	    }
	}
    }
    
    obj_allow_mods();

    if (timed_out > 0) {
	refresh_adminfile();
    }

    return STD_OK;
}
