/*	@(#)objects.h	1.3	94/04/06 11:51:11 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef _OBJECTS_H
#define _OBJECTS_H

#include "_ARGS.h"

#define OBJ_NONE	0x00
/* #define OBJ_ARCH	0x01 */
#define OBJ_POOL	0x02
#define OBJ_SUPER	0x04

typedef struct attribute {
    char             *attr_name;
    char             *attr_value;
    struct attribute *attr_next;
} attribute;

typedef struct object {
    short	obj_in_use;	/* private: set to 1 if object is in use  */
    short	obj_type;	/* one of the above OBJ_XXX		  */
    port	obj_check;	/* random check field			  */
    mutex	obj_mu;		/* avoid concurrent access to this object */
    attribute  *obj_attrlist;	/* pool parameters from admin file	  */

    struct pooldir *obj_pool;	/* only for OBJ_POOL type objects 	  */
    capability	obj_pooldir;	/* capability of corresponding directory  */

    int		obj_timeleft;	/* for garbage collection 		  */
} object;

#define OBJ_FRESH	10	/* initialisation of timeleft field */

/*
 * Initialisation, must be called once before any of the others.
 */
void    obj_init    _ARGS((void));

/*
 * Initialise object administration from previously dumped file.
 * The supercap found in the publishing directory is checked against the
 * one found in the adminstration.
 */
int	obj_read    _ARGS((char *dir, char *name, capability *supercap));

/*
 * Find, lock and validate an object specified by a private part.
 */
errstat obj_find    _ARGS((private *prv, int allowed_types,
			   rights_bits required, object **objp));

/*
 * Create a new object and return a capability to publishing it.
 */
errstat obj_new     _ARGS((capability *putcap, int type, struct pooldir *p));

/*
 * Store all currently known objects to disk.  The calling thread
 * should not have any object locked, or deadlock will result!
 */
int     obj_store   _ARGS((char *dir, char *name));

/*
 * Release an object obtained by obj_find() or obj_new()
 * Also reinitialises it's time-to-live field.
 */
void	obj_release _ARGS((object *obj));

/*
 * Add permanent attribute to an object.
 * It is saved to disk by obj_store(), and recovered by obj_read().
 * The string arguments are copied.
 * Returns STD_OK or STD_NOSPACE.
 */
errstat obj_set_attribute _ARGS((object *obj, char *name, char *value));

errstat new_super_obj _ARGS((capability *supercap));

/*
 * Delete and release a locked object.
 * To avoid race conditions, it must be done in the following way:
 *
 * obj_disallow_mods();
 * obj_find(.., &obj);
 * obj_delete(&obj);
 * obj_allow_mods();
 */
void	obj_delete	  _ARGS((object **obj));
void	obj_disallow_mods _ARGS((void));
void	obj_allow_mods	  _ARGS((void));

#endif /* _OBJECTS_H */


