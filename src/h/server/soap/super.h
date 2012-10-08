/*	@(#)super.h	1.3	96/02/27 10:38:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SUPER_H__
#define __SUPER_H__

/*
 * This is configuration data:
 */
#define L2VBLKSZ	9		/* log 2 of virtual block size */

/*
 * Some convenient definitions:
 */
#define BLOCKSIZE	(1 << L2VBLKSZ)
#define PAIRSPERBLOCK	(BLOCKSIZE / sizeof(sp_cappair))
#define MAXINTENT	((BLOCKSIZE - sizeof(sp_header)) / sizeof(sp_new))

/*
 * Old and new magic number of super files:
 */
#define SP_MAGIC_V0	0x05486217	/* old format */
#define SP_MAGIC_V1	0x05486218	/* byte order independent */
#define SP_MAGIC_V2	0x05486219	/* includes seqno */
#define SP_MAGIC_V3	0x05486220	/* for group based soapsvr */

#ifdef SOAP_GROUP
#define SP_MAGIC	SP_MAGIC_V3
#else
#ifdef SOAP_DIR_SEQNR
#define SP_MAGIC	SP_MAGIC_V2
#else
#define SP_MAGIC	SP_MAGIC_V1
#endif
#endif

/*
 * Description of some data structures:
 */
struct sp_cappair {		/* a pair of capabilities */
    capability sc_caps[2];
};
typedef struct sp_cappair sp_cappair;

struct sp_new {			/* a single intention */
    objnum     sn_object;	/* which object is being updated */
    sp_cappair sn_caps;		/* capabilities of bullet files */
};
typedef struct sp_new sp_new;

struct sp_header {		/* header of the super file		*/
    int32      sh_magic;	/* magic number of super file		*/
    union {
	/* we only include sh_super for compatibility with the old format */
        sp_cappair sh_super;	/* vdisk server caps (V0/V1; useless)   */
	struct {		/* for version >=2 we keep useful info here: */
           sp_seqno_t sh_dirseq; /* highest directory seqno */
	   int32      sh_modoff; /* offset in modlist vdisk or -1 */
	   capability sh_modcap; /* capability of modlist vdisk (if used) */
	} sh_extra_v2;
    } sh_extra;
    sp_cappair sh_private;	/* private dir capabilities		*/
    capability sh_public;	/* public dir capability		*/
    int32      sh_seq;		/* sequence number			*/
    int16      sh_copymode;	/* 1 or 2 copy mode; conf vec for group	*/
    int16      sh_nblocks;	/* size of super file			*/
    int16      sh_nintent;	/* # intentions				*/
};
typedef struct sp_header sp_header;

union sp_block {		/* description of a block */
    char          sb_block[BLOCKSIZE];
    sp_cappair    sb_cappairs[PAIRSPERBLOCK];
    struct {
	sp_header sc_header;
	sp_new    sc_intent[MAXINTENT];
    } sb_commit;
};
typedef union sp_block sp_block;

/*
 * More convenient definitions:
 */
#define sb_magic	sb_commit.sc_header.sh_magic
#define sb_super	sb_commit.sc_header.sh_extra.sh_super
#define sb_dirseq	sb_commit.sc_header.sh_extra.sh_extra_v2.sh_dirseq
#define sb_modoff	sb_commit.sc_header.sh_extra.sh_extra_v2.sh_modoff
#define sb_modcap	sb_commit.sc_header.sh_extra.sh_extra_v2.sh_modcap
#define sb_private	sb_commit.sc_header.sh_private
#define sb_public	sb_commit.sc_header.sh_public
#define sb_seq		sb_commit.sc_header.sh_seq
#define sb_copymode	sb_commit.sc_header.sh_copymode
#define sb_nblocks	sb_commit.sc_header.sh_nblocks
#define sb_nintent	sb_commit.sc_header.sh_nintent
#define sb_intent	sb_commit.sc_intent

#endif /* __SUPER_H__ */
