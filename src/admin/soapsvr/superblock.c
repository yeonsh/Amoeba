/*	@(#)superblock.c	1.5	96/02/27 10:23:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "soap/super.h"
#include "module/buffers.h"
#include "module/ar.h"
#include "module/prv.h"
#include "superblock.h"
#include "dirmarshall.h"
#include "diag.h"
#include "seqno.h"

#ifdef MAKESUPER
#define sp_whoami()	0
#endif

#ifdef DEBUG
#define dbprintf(list)	printf list
#else
#define dbprintf(list)	/**/
#endif

/* Marshalling and unmarshalling code for Soap's superblock.
 * Unmarshalling is non-trivial since the superblock originally
 * was both endian and compiler (!) dependent.
 * Support for recognising old-fashioned superblocks is only provided
 * when macro VERSION0_SUPPORT is defined.
 */
#ifdef AMOEBA
#define VERSION0_SUPPORT
#endif

#ifdef VERSION0_SUPPORT
#include "byteorder.h"

static errstat v0_unmarshall_super _ARGS((char *, char *, sp_block *));
#endif

static int
get_version(magic)
long magic;
{
    switch (magic) {
    case SP_MAGIC_V0:	return 0;	/* old-fashioned/endian dependent */
    case SP_MAGIC_V1:	return 1;
    case SP_MAGIC_V2:	return 2;
    case SP_MAGIC_V3:	return 3;
    }

    /* unknown: */
    return -1;
}

/*
 * Originally the superblock contained two super capabilities:
 *	sb_private.sc_caps[0] for Soap 0 and
 *	sb_private.sc_caps[1] for Soap 1
 * For the Group Directory server there are potentially 3 (or more) members,
 * so we cannot store them all (nor is that needed).  What we do have to
 * solve is to determine which is the servers "own" capability.
 *
 * This is determined as follows: for superblocks with version < 3,
 * capability 0 is always for member 0, and capability 1 is for members
 * 1 and higher.  For superblocks of version 2 and higher, the servers "own"
 * cap is already in position 0.  For the group version, irrespective of
 * the version of the superblock, the unmarshalled superblock will have
 * the server's own supercap in position 0.
 *
 * Hence the following definitions:
 */
#ifdef SOAP_GROUP
#define illegal_copy_mode(cm)	((cm) < 1)
#define caps_need_swap(me, v)	(((me) >= 1) && ((v) < 3))
#else
#define illegal_copy_mode(cm)	((cm) < 1 || (cm) > 2)
#define caps_need_swap(me, v)	((me) == 1 && ((v) >= 3))
#endif

static int
need_swap(whoami, version)
int whoami;
int version;
/* return 1 or 0 depending on whether we need to swap the super caps or not */
{
    return caps_need_swap(whoami, version);
}

static errstat
check_header(super)
sp_block *super;
{
#   define badsuper(s) { err = STD_ARGBAD; printf s; }
    errstat err = STD_OK;

    if (NULLPORT(&super->sb_private.sc_caps[0].cap_port) &&
	NULLPORT(&super->sb_private.sc_caps[1].cap_port))
    {
	badsuper(("both super capabilities are null!\n"));
    }
    if (NULLPORT(&super->sb_public.cap_port)) {
	badsuper(("public capability is null!\n"));
    }
    if (illegal_copy_mode(super->sb_copymode)) {
	badsuper(("illegal copymode: %d!\n", super->sb_copymode));
    }
    if (super->sb_nblocks < 2 || super->sb_nblocks > 20000) {
	badsuper(("illegal nblock: %d!\n", super->sb_nblocks));
    }
    if (super->sb_nintent < 0 || super->sb_nintent > MAXINTENT) {
	badsuper(("illegal number of intentions: %d!\n", super->sb_nintent));
    }
	
    return err;
}

#ifdef SOAP_DIR_SEQNR
static errstat
sp_init_v2(super)
sp_block *super;
{
    /* set default values for version 2 specific fields in superblock */
    static capability nullcap;

    zero_seq_no(&super->sb_dirseq);
    super->sb_modoff = -1;
    super->sb_modcap = nullcap;
}
#endif

errstat
sp_unmarshall_super(buf, end, super)
char     *buf;
char     *end;
sp_block *super;
{
    int     version;
    char   *p, *save_p;
    int     i;
    int     needswap;
    errstat err;
    
    /* First get the magic number */
    p = buf;
    p = buf_get_int32(p, end, &super->sb_magic);
    if (p == NULL) {
	return STD_SYSERR;
    }

    /* Determine the version of the superfile */
    version = get_version(super->sb_magic);
    if (version < 1) {
#ifdef VERSION0_SUPPORT
	return v0_unmarshall_super(buf, end, super);
#else
	printf("unrecognised magic number 0x%x\n", super->sb_magic);
	return STD_ARGBAD;
#endif
    }

    needswap = need_swap(sp_whoami(), version);
    dbprintf(("needswap = %d (whoami = %d, version = %d)\n",
	      needswap, sp_whoami(), version));
#ifdef SOAP_DIR_SEQNR
    /* first skip until after the cappair (which may in fact not be present) */
    if (version >= 2) {
	save_p = p;
    }
#endif
    p = buf_get_cap  (p, end, &super->sb_super.sc_caps[0]);
    p = buf_get_cap  (p, end, &super->sb_super.sc_caps[1]);
#ifdef SOAP_DIR_SEQNR
    if (version >= 2) {
	/* the union really contains useful extra info */
	save_p = buf_get_seqno(save_p, end, &super->sb_dirseq);
	save_p = buf_get_int32(save_p, end, &super->sb_modoff);
	save_p = buf_get_cap  (save_p, end, &super->sb_modcap);
    } else {
	sp_init_v2(super);
    }
#endif
    p = buf_get_cap  (p, end, &super->sb_private.sc_caps[needswap]);
    dbprintf(("supercap[%d] = %s\n", needswap,
	      ar_cap(&super->sb_private.sc_caps[needswap])));
    p = buf_get_cap  (p, end, &super->sb_private.sc_caps[1 - needswap]);
    dbprintf(("supercap[%d] = %s\n", 1 - needswap,
	      ar_cap(&super->sb_private.sc_caps[1 - needswap])));
    p = buf_get_cap  (p, end, &super->sb_public);
    p = buf_get_int32(p, end, &super->sb_seq);
    p = buf_get_int16(p, end, &super->sb_copymode);
    p = buf_get_int16(p, end, &super->sb_nblocks);
    p = buf_get_int16(p, end, &super->sb_nintent);

    if (p == NULL) {
	return STD_SYSERR;
    }

    /* perform misc. sanity checks before unmarshalling the intentions list */
    if ((err = check_header(super)) != STD_OK) {
	return err;
    }

    for (i = 0; i < super->sb_nintent; i++) {
	p = buf_get_int32(p, end, &super->sb_intent[i].sn_object);
	p = buf_get_cap  (p, end, &super->sb_intent[i].sn_caps.sc_caps[0]);
	p = buf_get_cap  (p, end, &super->sb_intent[i].sn_caps.sc_caps[1]);
    }

    if (p == NULL) {
	return STD_SYSERR;
    }

    return STD_OK;
}

errstat
sp_marshall_super(buf, end, super)
char     *buf;
char	 *end;
sp_block *super;
{
    int   i;
    char *p, *save_p;

    p = buf;
    p = buf_put_int32(p, end, (int32) SP_MAGIC);
#ifdef SOAP_DIR_SEQNR
    save_p = p;
#endif
    p = buf_put_cap  (p, end, &super->sb_super.sc_caps[0]);
    p = buf_put_cap  (p, end, &super->sb_super.sc_caps[1]);
#ifdef SOAP_DIR_SEQNR
    save_p = buf_put_seqno(save_p, end, &super->sb_dirseq);
#endif
    p = buf_put_cap  (p, end, &super->sb_private.sc_caps[0]);
    p = buf_put_cap  (p, end, &super->sb_private.sc_caps[1]);
    p = buf_put_cap  (p, end, &super->sb_public);
    p = buf_put_int32(p, end, super->sb_seq);
    p = buf_put_int16(p, end, super->sb_copymode);
    p = buf_put_int16(p, end, super->sb_nblocks);
    p = buf_put_int16(p, end, super->sb_nintent);

    for (i = 0; i < super->sb_nintent; i++) {
	p = buf_put_int32(p, end, super->sb_intent[i].sn_object);
	p = buf_put_cap  (p, end, &super->sb_intent[i].sn_caps.sc_caps[0]);
	p = buf_put_cap  (p, end, &super->sb_intent[i].sn_caps.sc_caps[1]);
    }
    
    if (p == NULL) {
	return STD_SYSERR;
    }

    return STD_OK;
}


/*
 *
 * The rest of this file only deals with the problem of unmarshalling
 * old-fashioned version 0 super blocks.  Only look at it at own risk!!
 *
 */

#ifdef VERSION0_SUPPORT

#define ENDIAN_BIG	0
#define ENDIAN_LITTLE	1

static char *
cvt_int32(p, end, endian, valptr)
char  *p;
char  *end;
int    endian;
int32 *valptr;
{
    if (p == NULL || (end - p) < sizeof(int32)) {
	return NULL;
    }
    (void) memcpy((_VOIDSTAR) valptr, (_VOIDSTAR) p, sizeof(int32));

    if (endian == ENDIAN_BIG) {
	dec_l_be(valptr);
    } else {
	dec_l_le(valptr);
    }
    return p + sizeof(int32);
}

static char *
cvt_int16(p, end, endian, valptr)
char  *p;
char  *end;
int    endian;
int16 *valptr;
{
    if (p == NULL || (end - p) < sizeof(int16)) {
	return NULL;
    }
    (void) memcpy((_VOIDSTAR) valptr, (_VOIDSTAR) p, sizeof(int16));
    
    if (endian == ENDIAN_BIG) {
	dec_s_be(valptr);
    } else {
	dec_s_le(valptr);
    }
    return p + sizeof(int16);
}

static char *
cvt_cap(p, end, endian, valptr)
char  *p;
char  *end;
int    endian;
capability *valptr;
/*ARGSUSED*/
{
    if (p == NULL || (end - p) < sizeof(capability)) {
	return NULL;
    }
    (void) memcpy((_VOIDSTAR) valptr, (_VOIDSTAR) p, sizeof(capability));

    /* capabilities are endian independent */
    return p + sizeof(capability);
}


static errstat
v0_check_intent(nr, intent, super)
int       nr;
sp_new   *intent;
sp_block *super;
{
#   define badintent(s) { err = STD_ARGBAD; printf s; }
    errstat err = STD_OK;
    objnum obj = intent->sn_object;
    objnum max_obj = (super->sb_nblocks - 1) * PAIRSPERBLOCK;

    if (obj < 0 || obj > max_obj) {
	badintent(("intention #%d: invalid object nr. %ld\n", nr, obj));
    } else if (obj == 0) {
	/* This would indicate an intention to replace one of the bullet
	 * servers.  Although it *is* possible, it is also highly
	 * unlikely, so we will not accept it while we are inspecting
	 * a version 0 file (which will be upgraded in a moment).
	 */
	badintent(("intention #%d: unexpected object 0\n", nr));
    } else {
	capability *caps = &intent->sn_caps.sc_caps[0];
	int j;

	for (j = 0; j < 2; j++) {
	    if (NULLPORT(&caps[j].cap_port)) {
		/* Then it should be entirely zero: */
		if (!NULLPORT(&caps[j].cap_priv.prv_random)) {
		    badintent(("intention #%d: strange cap #%d: %s\n",
			       nr, j, ar_cap(&caps[j])));
		}
	    } else {
		/* Then it should be a capability for a bullet directory file.
		 * Unfortunately we don't know the bullet servers
		 * being used (they are stored in block 1),
		 * so we'll just check that it has all rights.
		 */
		if ((caps[j].cap_priv.prv_rights & PRV_ALL_RIGHTS) !=
		    PRV_ALL_RIGHTS)
		{
		    badintent(("intention #%d: strange cap #%d: %s\n",
			       nr, j, ar_cap(&caps[j])));
		}
	    }
	}
    }

    return err;
}

static errstat
v0_unmarshall_intentlist(p, end, endian, super)
char     *p;
char     *end;
int       endian;
sp_block *super;
{
    int     i;
    errstat err = STD_OK;
    
    for (i = 0; i < super->sb_nintent; i++) {
	p = cvt_int32(p, end, endian, &super->sb_intent[i].sn_object);
	p = cvt_cap  (p, end, endian, &super->sb_intent[i].sn_caps.sc_caps[0]);
	p = cvt_cap  (p, end, endian, &super->sb_intent[i].sn_caps.sc_caps[1]);

	if (p == NULL) {
	    return STD_SYSERR;
	} else {
	    errstat e = v0_check_intent(i, &super->sb_intent[i], super);

	    if (e != STD_OK) {
		err = e;
	    }
	}
    }

    return err;
}

static errstat
v0_unmarshall_super(buf, end, super)
char     *buf;
char     *end;
sp_block *super;
{
    int32   magic;
    int     version;
    int     endian;
    int     needswap;
    char   *p;
    errstat err;
    
    /* First see what endian the superblock has: */
    endian = ENDIAN_BIG;
    (void) cvt_int32(buf, end, endian, &magic);
    version = get_version(magic);

    if (version < 0) {
	endian = ENDIAN_LITTLE;
	(void) cvt_int32(buf, end, endian, &magic);
	version = get_version(magic);
    }

    /* Not a version0 Soap super block, as far as we can see */
    if (version != 0) {
	printf("unrecognised magic number 0x%x\n", magic);
	return STD_ARGBAD;
    }

    /* see if we need swapping the super caps as well */
    needswap = need_swap(sp_whoami(), 0);

    p = buf;
    p = cvt_int32(p, end, endian, &super->sb_magic);
    p = cvt_cap  (p, end, endian, &super->sb_super.sc_caps[0]);
    p = cvt_cap  (p, end, endian, &super->sb_super.sc_caps[1]);
#ifdef SOAP_DIR_SEQNR
    sp_init_v2(super);
#endif
    p = cvt_cap  (p, end, endian, &super->sb_private.sc_caps[needswap]);
    p = cvt_cap  (p, end, endian, &super->sb_private.sc_caps[1 - needswap]);
    p = cvt_cap  (p, end, endian, &super->sb_public);
    p = cvt_int32(p, end, endian, &super->sb_seq);
    p = cvt_int16(p, end, endian, &super->sb_copymode);
    p = cvt_int16(p, end, endian, &super->sb_nblocks);
    p = cvt_int16(p, end, endian, &super->sb_nintent);

    if (p == NULL) {
	return STD_SYSERR;
    }

    /* perform misc. sanity checks before unmarshalling the intentions list */
    if ((err = check_header(super)) != STD_OK) {
	return err;
    }

    /* In version 0 superfiles, for alignment reasons (or otherwise)
     * a gap of size short may have been introduced between sp_header
     * and the intentions list.
     * In case conversion according to the most likely alternative fails,
     * we have to try the other as well.
     */
    {
	struct check {
	    struct { int16 s1; } s2;
	    int32 s3;
	};
	char *try1, *try2;

	if (sizeof(struct check) > sizeof(int16) + sizeof(int32)) {
	    /* apparently, this compiler/architecture introduces gaps */
	    try1 = p + sizeof(int16);
	    try2 = p;
	} else {
	    /* try gapless version first */
	    try1 = p;
	    try2 = p + sizeof(int16);
	}

	err = v0_unmarshall_intentlist(try1, end, endian, super);

	if (err == STD_ARGBAD) {
	    printf("encountered old-fashioned version 0 super block\n");
	    printf("entering auto-recovery mode ...\n");
	    err = v0_unmarshall_intentlist(try2, end, endian, super);
	}

	if (err == STD_OK) {
	    printf("succesfully read an old-style super block\n");
	}
    }

    return err;
}

#endif	/* VERSION0_SUPPORT */

