/*	@(#)mhdr.c	1.4	94/04/07 14:33:47 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include <amoeba.h>
# include "ail.h"

/*
 *	Versions of the important Amoeba system types.
 *	Note: previous (pre-Amoeba 4) versions have been removed to avoid
 *	confusion, so in fact there's only one supported currently.
 */
char *v_name[] = {
    "3.5",	/* Client stubs and mainloops return errstat	*/
    "4.0"	/* New server interface for arrays and malloced req buf */
};

#define V_3_5	0
#define V_4_0	1
#define N_VER	(sizeof(v_name)/sizeof(v_name[0]))

static int version = DFLT_VERSION;
char *client_stub_return_type = "errstat";
char *mainloop_return_type = "errstat";

int Do_malloc_reqbuf = 0;
int Do_pass_arraysize = 0;

void
SetVersion(s)
    char *s;
{
    static Bool called = No;
    if (!called) {
	called = Yes;
    } else {
	mypf(ERR_HANDLE, "Please choose ONE version\n");
	exit(1);
    }
    for (version = 0; version < N_VER; version++) {
	if (strcmp(s, v_name[version]) == 0)
	    return;
    }
    mypf(ERR_HANDLE, "Unknown version %s; ", s);
    if (N_VER == 1) {
	mypf(ERR_HANDLE, "only version supported is %s\n", v_name[0]);
    } else {
	mypf(ERR_HANDLE, "choose one of");
	for (version = 0; version < N_VER; version++)
	    mypf(ERR_HANDLE, version == N_VER -1 ? " or %s\n" : " %s,",
		 v_name[version]);
    }
    exit(1);
} /* SetVersion */

/*
 *	This file knows about the extra
 *	fields in the amoeba header.
 *	Currently these fields are known:
 *	h_offset:		long;
 *	h_size, h_extra:	unsigned short;
 *	h_port:			struct port;
 *	h_private:		struct private;
 *
 *	Use this as follows: when attempting to lay-out a message,
 *	first thing to do is AmEmpty(), i.e. state that there are
 *	NO fields assigned yet. Then all the user-assignments are
 *	checked by AmOk(), which makes amok if it doesn't agree.
 *	All other fields can be located by repeatedly calling AmWhere().
 *
 *	To be able to store capabilities in the header we have to
 *	do a trick, since there is actually no capability in the
 *	header. Note that because of this, the marshaling generators
 *	have to be aware of this as well!
 *
 *	And then there is this obfuscated trick that I use to be able
 *	to put shorts in a long, if that's all there's left in the header.
 *	AmComp() says ALMOST in this case. The routine to layout a
 *	message had better not ignore this when a parameter has the VAR-bit
 *	set, because passing the address of a long where the impl_ expects
 *	a short* is not what you want. So AmComp sets a global flag.
 */
# define N_FLDS	5		/* 5ive header fields	*/
static struct am_field {	/* Description of hdr	*/
    char *am_name;		/* Name of the field.	*/
    struct typdesc *am_type;	/* Type of the field.	*/
    int am_used;		/* This one used yet?	*/
} fields[N_FLDS];

/*
 *	Shorthands for port, private:
 */
#define FIELD_PORT	3
#define FIELD_PRIV	4

Bool am_almost;		/* Fits almost; not exact	*/
static Bool am_replcap;	/* Capability in the reply	*/
int am_leftinint;	/* #in-ints left in the header	*/
/* This text should only show up in comments:		*/
char *pseudofield = "h_port and h_priv";

/*
 *	Return the type of a headerfield
 */
struct typdesc *
AmType(s)
    char *s;
{
    int i = N_FLDS;
    while (i--) if (s == fields[i].am_name) return fields[i].am_type;
#ifdef PYTHON
    /*
    **	The pseudofield does not have a real type but for the Python
    **	stubcode generator we need some thing to return so return NULL.
    */
    if(!strcmp(pseudofield, s)) return NULL;
#endif /* PYTHON */
    assert(No);	/* Better use what _I_ said	*/
    /*NOTREACHED*/
} /* AmType */

/*
 *	Initialise the fields.
 *	Note that ail knows about capability,
 *	and therefor port private, but not header.
 *	This routine knows what capabilities and headers look like.
 */
void
AmInit()
{
    struct class *OldClass;
    struct itlist *itl;

    pseudofield = StrFind(pseudofield);	/* So we can pointer-compare	*/
    OldClass = ThisClass;
    ThisClass = &GlobClass;		/* port and private are global	*/
    switch (version) {			/* Not every Amoeba is the same	*/
    case V_4_0:
	Do_malloc_reqbuf = 1;
	Do_pass_arraysize = 1;
	/* fallthrough */
    case V_3_5:
    {
	struct typdesc *i32, *ui16, *i8, *ui8;	/* Several integers	*/
	struct typdesc *tmp;			/* scratch		*/
	struct typdesc *porttyp, *privtyp;	/* "port", "private"	*/


	/* All kinds of renaming:	*/
	i8 = NewTD(StrFind("int8"), ModifTyp(tk_char, 0, 0, 0));
	ui8 = NewTD(StrFind("uint8"), ModifTyp(tk_char, 0, 0, 1));
	ui16 = NewTD(StrFind("uint16"), ModifTyp(tk_int, 0, 1, 1));
	i32 = NewTD(StrFind("int23"), ModifTyp(tk_int, 1, 0, 0));

	/* For the 'port' type:		*/
	tmp = ArrayOf(i8, Cleaf((long) PORTSIZE), Cleaf((long) PORTSIZE));
	itl = it(StrFind("_portbytes"), tmp);
	tmp = DefSU(tk_struct, (char *) NULL, itl);
	porttyp = NewTD(StrFind("port"), tmp);

	/* The 'private' type:		*/
	tmp = ArrayOf(i8, Cleaf((long) 3), Cleaf((long) 3));
	itl = it(StrFind("prv_object"), tmp);
	itl = itcat(itl, it(StrFind("prv_rights"), ui8));
	itl = itcat(itl, it(StrFind("prv_random"), porttyp));
	tmp = DefSU(tk_struct, (char *) NULL, itl);
	privtyp = NewTD(StrFind("private"), tmp);

	/* Construct the 'capability' type:	*/
	itl = it(StrFind("cap_port"), porttyp);
	itl = itcat(itl, it(StrFind("cap_priv"), privtyp));
	tmp = DefSU(tk_struct, (char *) NULL, itl);
	cap_typ = NewTD(StrFind("capability"), tmp);

	fields[0].am_name = StrFind("h_extra");
	fields[0].am_type = ui16;
	fields[1].am_name = StrFind("h_size");
	fields[1].am_type = NewTD(StrFind("bufsize"), ui16);
	fields[2].am_name = StrFind("h_offset");
	fields[2].am_type = i32;
	fields[FIELD_PORT].am_name = StrFind("h_port");
	fields[FIELD_PORT].am_type = porttyp;
	fields[FIELD_PRIV].am_name = StrFind("h_private");
	fields[FIELD_PRIV].am_type = privtyp;
	break;
    }

    default:
	mypf(ERR_HANDLE, "Version %s not implemented\n", v_name[version]);
	exit(1);
    }

    ThisClass = OldClass;
} /* AmInit */

/*
 *	Mark all fields as unused, except for
 *	h_port and h_private, which are used
 *	to ship the '*' parameter at the request.
 *	The order in which the integer fields are
 *	listed is significant. The shortest are
 *	listed first, because you don't want to
 *	put a short in h_offset and then discover
 *	that you can't put a long in the header anymore.
 */
void
AmEmpty()
{
    fields[0].am_used = 0;		/* h_extra	*/
    fields[1].am_used = 0;		/* h_size	*/
    fields[2].am_used = 0;		/* h_offset	*/
    fields[FIELD_PORT].am_used = AT_REQUEST;
    fields[FIELD_PRIV].am_used = AT_REQUEST;
    am_leftinint = 3;		/* Got 3 request ints	*/
    am_replcap = No;		/* Didn't use cap	*/
} /* AmEmpty */

/*
 *	Check if a parameter of type pt can be put in a
 *	header-field with type ft. This is done in order to
 *	put shorts in an unshort field, whether they are
 *	signed or unsigned.
 */
/* static */ Bool
AmComp(pt, ft)
    struct typdesc *ft, *pt;	/* Field- and parametertype */
{
    am_almost = No;

    /* Ignore typedefs and self-generated pointertypes	*/
    while (ft->td_kind == tk_ref || ft->td_kind == tk_tdef) {
	ft = ft->td_prim;
	assert(ft != NULL);
    }

    while (pt->td_kind == tk_ref || pt->td_kind == tk_tdef)
	pt = pt->td_prim;

    /*		Hack for enumerated types.
     * This should be done smarter: some enums are small enough for
     * short headerfields, but Amoeba 6.0 won't contains short fields, so...
     */

    if (pt->td_kind == tk_enum) {
	am_almost = Yes;	/* Enforce casting */
	pt = ModifTyp(tk_int, 0, 1, 1);
    }

    if (ft == pt) return Yes;			/* They are the same	*/
    if (ft->td_kind != pt->td_kind) return No;	/* Very different	*/
    if (ft->td_kind == tk_int && ft->td_size >= pt->td_size) {
	/* Integer of sufficient size	*/
	am_almost = Yes;
	return Yes;
    }
#ifdef INTS_CAN_BE_SHORT
    /* Exception for plain int: legal to put it in a short	*/
    if (pt->td_kind == tk_int && pt->td_size == 0 && ft->td_size == -1) {
	/* Integer is put in the header	*/
	am_almost = Yes;
	return Yes;
    }
#endif
    return No;
} /* AmComp */

/*
 *	Check whether this field can be used
 *	to marshal a parameter of this type
 *	in the specified direction.
 */
void
AmOk(name, type, dir)
    char *name;
    struct typdesc *type;
    int dir;	/* Direction, contains two bits	*/
{
    int i;
    assert(dir != 0);
    /* Find the field - it might be nonsense */
    for (i = 0; i<N_FLDS; ++i) if (fields[i].am_name == name) {
	/* Found the field	*/
	if (!AmComp(type, fields[i].am_type)) {
	    mypf(ERR_HANDLE, "%r typeconflict in field %s\n", name);
	}
	if (fields[i].am_used & dir) {
	    mypf(ERR_HANDLE, "%r Attempt to re-use %s\n", name);
	} else {
	    fields[i].am_used |= dir;
	    if (type->td_kind == tk_int && (dir & AT_REQUEST))
		--am_leftinint;
	}
	return;
    }
    mypf(ERR_HANDLE, "%r %s is not a member of the message-header\n", name);
} /* AmOk */

/*
 *	Try to locate a field with the appropiate type,
 *	return its name if there is one, NULL if not.
 */
char *
AmWhere(type, dir)
    struct typdesc *type;
    int dir;
{
    int i;
    /* Exception for capability in the reply:	*/
    if (type == cap_typ && dir == AT_REPLY) {
	/* Are those two fields still free?	*/
	if ((fields[FIELD_PORT].am_used & AT_REPLY) ||
	    (fields[FIELD_PRIV].am_used & AT_REPLY))
	    return NULL;
	/* We got space left, mark port & private as used;	*/
	fields[FIELD_PORT].am_used |= AT_REPLY;
	fields[FIELD_PRIV].am_used |= AT_REPLY;
	/* Force redeclaration in server	*/
	am_almost = Yes;
	return pseudofield;
    }
    for (i = 0; i < N_FLDS; ++i) {
	if (((fields[i].am_used & dir)==0) && AmComp(type, fields[i].am_type)) {
	    /* Found one */
	    fields[i].am_used |= dir;
	    /*XXX Fix by Jelke: make sure we count typedeffed ints */
	    if ( SameType(type)->td_kind == tk_int
	    		&& (dir & AT_REQUEST)) --am_leftinint;
	    return fields[i].am_name;
	}
    }
    /* None found */
    return (char *) NULL;
} /* AmWhere */

#ifdef PYTHON
/*
 *	Return index of header member
 *	Currently, only the PYTHON part uses this
 */
int
AmFind(member)
    char *member;
{
    int i;
    for (i = 0; i < SIZEOFTABLE(fields); ++i)
	if (member == fields[i].am_name) return i;
    if (!strcmp(member, pseudofield)) return i;
    assert(0);	/* The name MUST be there */
} /* AmFind */
#endif
