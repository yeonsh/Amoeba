/*	@(#)gmakesuper.c	1.1	96/02/27 10:01:22 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** MAKESUPER -- Create super files for the SOAP directory servers.
**
** This version runs both under Unix and Amoeba.
**
** When bootstrapping from Unix it uses the ability of super_host_lookup
** to find capabilities without the help of any directory server,
** and saves the capabilities it creates on Unix files.
**
** As an example, to set up a replicated soap service using host "foo"
** for server 0, with the super file on vdisk:03, and using host "bar"
** for server 1, with the super file on vdisk:03, and 100 disk blocks
** (i.e., 51200 bytes or 50K) devoted to the super files, you type:
**
**	makesuper foo/bullet bar/bullet foo/vdisk:03 bar/vdisk:03 100
**
** To create a single-server soap service on host "foo" using vdisk:03,
** (again with 100 blocks) you type:
**
**	makesuper foo/bullet - foo/vdisk:03 - 100
**
** If you decide later on that you want a duplicated service after all,
** you can use the command
**
**	makesuper -A foo/bullet bar/bullet foo/vdisk:03 bar/vdisk:03
**
** to initialize the superfile for the second service on bar/vdisk:03.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "byteorder.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "soap/super.h"
#include "bullet/bullet.h"
#include "disk/disk.h"
#include "module/name.h"
#include "module/direct.h"
#include "module/disk.h"
#include "module/rnd.h"
#include "module/prv.h"
#include "module/buffers.h"
#include "module/host.h"
#ifndef AMOEBA
#include "amupolicy.h"
#endif

#define NBULLETS	2	/* .. and don't you dare to change this! */

#define MIN_BLOCKS	3	/* require at least three blocks */
#define DEF_MAX_BLOCKS  2000	/* restrict the default size	 */

#define SUPER_CAP0	"super_cs.0"
#define SUPER_CAP1	"super_cs.1"
#define SUPER_CAP_NEW	"super_cs.new"
#define DEF_NEW_ROOT	"new_root"

static capability Null_cap;

/*
 * Abort with error message.
 */

static char *Prog_name = "makesuper";

static void
panic3(format, s1, s2)
char *format;
char *s1;
char *s2;
{
    fprintf(stderr, "%s: fatal: ", Prog_name);
    fprintf(stderr, format, s1, s2);
    fprintf(stderr, "\n");
    exit(1);
}

static void
panic(msg)
char *msg;
{
    panic3("%s", msg, "");
}

/*
 * Allocate, initialize and create replicas for a new, empty directory.
 * Initialises the private part for the dircap corresponding to it,
 * and returns the Bullet capabilities storing the directory information.
 */
static void
create_dir(dircap, obj_nr, bullet_svr, bullet_file)
capability *dircap;
objnum	    obj_nr;
capability  bullet_svr[NBULLETS];
capability  bullet_file[NBULLETS];
{
    extern char   *buf_put_dir();
    static char   *def_colnames[] = { "owner", "group", "other", 0 };
    struct sp_dir *sd;
    char 	   dirbuf[1024];
    char 	  *bufp;
    b_fsize	   size;
    errstat	   err;
    int		   bulnr;
    int 	   commit_flags = BS_COMMIT | BS_SAFETY;

    sd = (struct sp_dir *) calloc(sizeof(struct sp_dir), 1);
    if (sd == NULL) {
	panic("could not allocate directory");
    }

    uniqport(&sd->sd_random);
    if (prv_encode(&dircap->cap_priv, obj_nr,
		   PRV_ALL_RIGHTS, &sd->sd_random) < 0)
    {
	panic("prv_encode for new directory failed");
    }

    /* initialize columns of the directory */
    sd->sd_ncolumns = 3;
    sd->sd_colnames = def_colnames;

    /* initialize rows of the directory */
    sd->sd_nrows = 0;
    sd->sd_rows = NULL;

#ifdef SOAP_GROUP
    /* Set initial seqno [0, 1].
     * (note: not [0,0] because that indicates an absent directory).
     */
    sd->sd_seq_no.seq[0] = 0;
    sd->sd_seq_no.seq[1] = 1;
#endif

    bufp = buf_put_dir(dirbuf, dirbuf + sizeof(dirbuf), sd);
    if (bufp == NULL) {
	panic("cannot put directory in buffer");
    }
    size = bufp - dirbuf;

    /*
     * create bullet files, or zero caps.
     */
    for (bulnr = 0; bulnr < NBULLETS; bulnr++) {
	if (NULLPORT(&bullet_svr[bulnr].cap_port)) {
	    bullet_file[bulnr] = Null_cap;
	} else {
	    err = b_create(&bullet_svr[bulnr], dirbuf, size,
			   commit_flags, &bullet_file[bulnr]);
	    if (err != STD_OK) {
		fprintf(stderr, "cannot create bullet %d directory file (%s)",
			bulnr, err_why(err));
		panic("make sure the bullet server is up");
	    }
	}
    }

    free((_VOIDSTAR) sd);
}

/*
 * Invent a random get capability and create corresponding put capability.
 * Note: Soap should really store *get ports* and *check fields* rather than
 * *get capabilities* in block 0.
 */
static void
makecap(getcap, putcap)
capability *getcap;
capability *putcap;
{
    port check;

    /*
     * Create capability with get port
     */
    uniqport(&getcap->cap_port);
    uniqport(&check);
    if (prv_encode(&getcap->cap_priv, (objnum) 0,
		   PRV_ALL_RIGHTS, &check) < 0)
    {
	/* only fails when too many rights are asked */
	panic("prv_encode failed");
    }

    /*
     * Convert get capability to put capability
     */
    putcap->cap_priv = getcap->cap_priv;
    priv2pub(&getcap->cap_port, &putcap->cap_port);
}

static void
store_buf(name, mode, buf, size)
char 	*name;
unsigned mode;
char    *buf;
int      size;
{
    int fd;

    if ((fd = creat(name, (unsigned short) mode)) < 0) {
	perror(name);
	panic("creat failed");
    }

    if (write(fd, buf, size) != size) {
	perror(name);
	panic("write failed");
    }

    (void) close(fd);
}

static void
store_cs_cap(name, cap)
char *name;
capability *cap;
{
    /*
     * Convert capability to a capset and store it in a file.
     * Later on it can be installed under Soap using the command
     *		put amoeba_name < capset_file
     */
    char  *p;
    char   buf[40];	/* big enough for singleton capset */
    capset cs;
	
    /* make a capset from the put capability and put it into a buffer */
    if (!cs_singleton(&cs, cap)) {
	panic("cannot create capability set");
    }

    p = buf_put_capset(buf, buf + sizeof(buf), &cs);
    if (p == NULL) {
	panic("cannot fit capability set in buffer");
    }

    store_buf(name, 0600, buf, (int)(p - buf));
}

static errstat
my_sp_lookup(dircap, name, cap)
capability *dircap;
char 	   *name;
capability *cap;
{
    /* Simple version of the dir_lookup/sp_lookup stubs.
     * Doesn't call sp_init(), requiring a ".capability" file under Unix.
     */
    bufsize n;
    header  hdr;
    char   *left;
    char   *bufp, *end;
    capset  cs;
    char    cs_buf[200];
    errstat err;
    
    hdr.h_port = dircap->cap_port;
    hdr.h_priv = dircap->cap_priv;
    hdr.h_command = SP_LOOKUP;
    hdr.h_size = strlen(name) + 1;
    n = trans(&hdr, name, hdr.h_size, &hdr, cs_buf, (bufsize) sizeof(cs_buf));

    if (ERR_STATUS(n)) {
	err = ERR_CONVERT(n);
    } else {
	err = ERR_CONVERT(hdr.h_status);
    }
    if (err != STD_OK) {
	return err;
    }

    bufp = cs_buf;
    end = &cs_buf[n];

    bufp = buf_get_string(bufp, end, &left);
    if (bufp == NULL) {
	return STD_SYSERR;	/* bad reply format */
    }
    if (*left != '\0') {
	return STD_NOTFOUND;	/* we don't iterate like in the default stub */
    }
    
    if (buf_get_capset(bufp, end, &cs) == NULL) {
	return STD_NOSPACE;
    }

    err = cs_to_cap(&cs, cap);
    cs_free(&cs);

    return err;
}

/*
 * Special lookup function.
 * If the path is relative, its first component is passed to super_host_lookup,
 * which (under Unix) knows to find host capabilities without the need
 * for a running directory server.
 */
static errstat
my_lookup(path, cap)
char *path;
capability *cap;
{
    capability  temp_cap;
    char	buf[256];
    char       *lookup;
    errstat     err;
    int		fd;
	
    if (*path == '/') {	/* absolute path */
	return name_lookup(path, cap);
    }

    /* Copy path in buf, and see if we still have to lookup a last component */
    (void) strcpy(buf, path);
    lookup = strchr(buf, '/');
    if (lookup != NULL) {
	/* remove last component from buf, and point lookup to it */
	*lookup++ = '\0';
    }

    err = STD_NOTFOUND;

    /* First try to read capability from a file */
    if ((fd = open(buf, 0)) >= 0) {
	char capbuf[sizeof(capability)];
	char *capbufend = capbuf + sizeof(capbuf);

	if ((read(fd, capbuf, sizeof(capability)) == sizeof(capability)) &&
	    (buf_get_cap(capbuf, capbufend, &temp_cap) != NULL))
	{
	    err = STD_OK;
	}
	close(fd);
    }

    /* If that failed, try super_host_lookup (which reads from another file) */
    if (err != STD_OK) {
	err = super_host_lookup(buf, &temp_cap);
    }

    /*
     * We still might have to look up the last component.
     * If we are bootstrapping the system from Unix, and there is no
     * ".capability" file yet, we cannot use the directory stubs,
     * because they try to initialise from that (in the function sp_init()).
     * So, instead we do the lookup transaction ourselves.
     */
    if (err == STD_OK) {
	if (lookup != NULL) {
	    err = my_sp_lookup(&temp_cap, lookup, cap);
	} else {
	    *cap = temp_cap;
	}
    }

    return err;
}

static void
set_super_caps(sp_blocks, nblocks, obj_nr, caps)
sp_block  *sp_blocks;
int	   nblocks;
objnum	   obj_nr;
capability caps[NBULLETS];
{
    /*
     * add the bullet capabilities for object obj_nr to the super file
     */
    long block_nr = 1 + (obj_nr / PAIRSPERBLOCK);
    long block_off = obj_nr % PAIRSPERBLOCK;
    int  i;

    if (block_nr > nblocks) {
	panic("set_super_caps: block number too big");
    }

    for (i = 0; i < NBULLETS; i++) {
	char *buf;

	buf = (char *) &sp_blocks[block_nr].sb_cappairs[block_off].sc_caps[i];
	(void) buf_put_cap(buf, buf + sizeof(capability), &caps[i]);
    }
}

static int
exists(name)
char *name;
{
#ifdef AMOEBA
    capability cap;
    errstat err;

    err = name_lookup(name, &cap);
    switch (err) {
    case STD_OK:	return 1;
    case STD_NOTFOUND:	return 0;
    default:		panic3("unexpected failure looking up \"%s\" (%s)",
			       name, err_why(err));
			/*NOTREACHED*/
    }
#else
    int fd;

    if ((fd = open(name, 0)) >= 0) {
	close(fd);
	return 1;
    } else {
	return 0;
    }
#endif
}

#define IS_SOAPMAGIC(m)	((m) == SP_MAGIC_V0 || (m) == SP_MAGIC_V1)

static errstat
has_superblock(disk_cap)
capability *disk_cap;
{
    /*
     * Returns STD_OK if the disk specified by disk_cap does not contain
     * a Soap super file.  If it does, STD_EXISTS is returned.
     * When the disk cannot be read, it returns an other error code.
     */
    sp_block super_block;
    long     check;
    errstat  err;
    
    err = disk_read(disk_cap, L2VBLKSZ,
		    (disk_addr) 0, (disk_addr) 1, (bufptr) &super_block);
    if (err != STD_OK) {
	return err;
    }

    /* We have to try both endiannesses.  Try big endian first */
    check = super_block.sb_magic;
    dec_l_be(&check);
    if (IS_SOAPMAGIC(check)) {
	return STD_EXISTS;
    }

    /* Little endian */
    check = super_block.sb_magic;
    dec_l_le(&check);
    if (IS_SOAPMAGIC(check)) {
	return STD_EXISTS;
    }

    /* Whatever it is, it's not a Soap super block */
    return STD_OK;
}

static void
check_superblock(disk_name, disk_cap)
char       *disk_name;
capability *disk_cap;
{
    /*
     * Checks that the disk indicated doesn't contain a Soap super file yet.
     * Otherwise it panics with an appropriate error message.
     */
    errstat err;

    err = has_superblock(disk_cap);

    switch (err) {
    case STD_OK:
	/* no Soap super block */
	return;
    case STD_EXISTS:
	panic3("vdisk `%s' already has a Soap super file (%s)",
	       disk_name, "use '-f' to overwrite");
	break;
    default:
	panic3("could not read superblock of vdisk `%s' (%s)",
	       disk_name, err_why(err));
	break;
    }
}

extern errstat sp_unmarshall_super();

static sp_block Super_block;	/* Block 0; a.k.a. commit block */

static void
add_modlist(vdisk, size)
char     *vdisk;
disk_addr size;
{
    char       blockbuf[BLOCKSIZE];
    capability vdisk_cap;
    disk_addr  nsuper_blocks;
    disk_addr  nblocks;
    errstat    err;

    if ((err = my_lookup(vdisk, &vdisk_cap)) != STD_OK) {
	panic3("cannot lookup vdisk capability \"%s\" (%s)",
	       vdisk, err_why(err));
    }

    /*
     * Sanity check: see if there is room for the mod list
     */
    err = disk_read(&vdisk_cap, L2VBLKSZ,
		    (disk_addr) 0, (disk_addr) 1, (bufptr) blockbuf);
    if (err != STD_OK) {
	panic3("cannot read the superblock from %s (%s)", vdisk, err_why(err));
    }
    
    err = sp_unmarshall_super(blockbuf, &blockbuf[BLOCKSIZE], &Super_block);
    if (err != STD_OK) {
	panic3("cannot unmarshall superblock of %s (%s)", vdisk, err_why(err));
    }

    nsuper_blocks = Super_block.sb_nblocks;
    printf("The superfile contains %ld blocks\n", nsuper_blocks);
    
    err = disk_size(&vdisk_cap, L2VBLKSZ, &nblocks);
    if (err != STD_OK) {
	panic3("cannot get size of %s (%s)", vdisk, err_why(err));
    }

    printf("The vdisk partition has %ld blocks\n", (long) nblocks);

    /*
     * If it fits at the end, add the mod list.
     */
    if (nsuper_blocks + size + 1 < nblocks) {
	err = md_blockformat(&vdisk_cap, nblocks - size - 1, size);
	if (err != STD_OK) {
	    panic3("error writing mod list to disk (%s)", err_why(err), "");
	}

	printf("wrote %ld + 1 modlist blocks to disk, startblock = %ld\n",
	       (long) size, (long) (nblocks - size - 1));
    } else {
	fprintf(stderr, "No room for %d + 1 modlist blocks\n", size);
	exit(1);
    }
}

static errstat
write_super(diskcap, superblock, sp_blocks, allocated, total)
capability *diskcap;
sp_block   *superblock;
sp_block   *sp_blocks;
int	    allocated;
disk_addr   total;
{
    extern errstat sp_marshall_super(); /* shared with soap server */
    static sp_block Null_block;
    disk_addr blknr;
    errstat   err;

    /*
     * Marshall the super block in sp_blocks[0]
     */
    err = sp_marshall_super((char *) &sp_blocks[0], (char *) &sp_blocks[1],
			    superblock);
    if (err != STD_OK) {
	fprintf(stderr, "cannot marshall the superblock (%s)\n", err_why(err));
	return err;
    }

    /*
     * First zero the remaining blocks
     */
    for (blknr = allocated; blknr < total; blknr++) {
	err = disk_write(diskcap, L2VBLKSZ,
			 blknr, (disk_addr) 1, (bufptr) &Null_block);

	if (err != STD_OK) {
	    fprintf(stderr, "cannot zero block %d (%s)\n", err_why(err));
	    return err;
	}
    }

    /*
     * Then write the allocated blocks following block 0
     */
    if (allocated > 1) {
	err = disk_write(diskcap, L2VBLKSZ,
			 (disk_addr) 1, (disk_addr) (allocated - 1),
			 (bufptr) &sp_blocks[1]);
	if (err != STD_OK) {
	    fprintf(stderr, "cannot write blocks %d - %d (%s)\n",
		    1, allocated - 1, err_why(err));
	    return err;
	}
    }

    /*
     * And finally write the marshalled superblock
     */
    err = disk_write(diskcap, L2VBLKSZ,
		     (disk_addr) 0, (disk_addr) 1, (bufptr) &sp_blocks[0]);
    if (err != STD_OK) {
	fprintf(stderr, "cannot write the superblock (%s)\n", err_why(err));
	return err;
    }

    return STD_OK;
}

static errstat
get_member_info(disk_cap, nblks, copy_mode, priv_getcap, pub_getcap)
capability *disk_cap;
disk_addr  *nblks;
int        *copy_mode;
capability *priv_getcap;
capability *pub_getcap;
{
    /* Read the superblock of an existing member and return requested info */
    char     buf[BLOCKSIZE];
    sp_block member;
    errstat  err;

    err = disk_read(disk_cap, L2VBLKSZ, (disk_addr) 0, (disk_addr) 1, buf);
    if (err != STD_OK) {
	fprintf(stderr, "cannot read existing superblock\n");
	return err;
    }

    err = sp_unmarshall_super(buf, buf + sizeof(buf), &member);
    if (err != STD_OK) {
	fprintf(stderr, "cannot unmarshall the existing superblock\n");
	return err;
    }

    *nblks = member.sb_nblocks;
    printf("nblocks = %d\n", *nblks);

    *copy_mode = member.sb_copymode;

    /* The existing member's private cap is the first of sb_private */
    *priv_getcap = member.sb_private.sc_caps[0];
    *pub_getcap = member.sb_public;

    return STD_OK;
}

/* Only allocate 2 soap super blocks; the rest will be zero */
#define ALLOC_BLOCKS	2

static void
create_superfiles(force, add_member, nblks, bullet_name, vdisk_name, root_name)
int       force;
int	  add_member;	/* are we adding a new member to an existing system? */
disk_addr nblks;
char     *bullet_name[];
char     *vdisk_name[];
char     *root_name;
{
    sp_block   *sp_blocks;
    capability  bullet_caps[NBULLETS];	/* server storing directories	    */
    capability  vdisk_caps[NBULLETS];	/* server storing super blocks	    */
    capability  priv_getcaps[2];	/* soap private capabiliy (getport) */
    capability  pub_putcap;		/* soap public cap (putport)   	    */
    capability  pub_getcap;		/* soap public cap (getport)	    */
    capability  dir_cap;		/* capability for new root dir      */
    disk_addr   size_0 = 0, size_1 = 0;
    int		vdisk1_valid = 0;	/* set to 1 if vdisk_caps[1] is valid*/
    errstat 	err;

    if (!add_member && root_name == NULL) {
#ifdef AMOEBA
	root_name = DEF_NEW_ROOT;
#else
	/* By default, create a ".capability" file. */
	static char dot_capability[256];    /* pathname of .capability file */
	char       *home;

	if ((home = getenv("HOME")) == NULL) {
	    panic("no environment variable \"HOME\"");
	}
	sprintf(dot_capability, "%s/%s", home, CAP_FILENAME);

	root_name = dot_capability;
#endif
    }

    /*
     * To avoid losing a vital reference to a possibly still existing
     * Soap system, refuse to overwrite existing root_name.
     */
    if (!add_member && exists(root_name)) {
	panic3("capability \"%s\" already exists (%s)",
	       root_name, "use '-c new_soap_cap'");
    }

    /*
     * Allocate the super blocks that will be nonzero.
     */
    sp_blocks = (sp_block *) calloc(sizeof(sp_block), (unsigned) ALLOC_BLOCKS);
    if (sp_blocks == NULL) {
	panic("cannot allocate super blocks");
    }

    /*
     * Get Bullet server capabilities (immutable file servers).
     */
    if ((err = my_lookup(bullet_name[0], &bullet_caps[0])) != STD_OK) {
	panic3("cannot lookup bullet 0 capability \"%s\" (%s)",
	       bullet_name[0], err_why(err));
    }

    if (strcmp(bullet_name[1], "-") == 0) {
	bullet_caps[1] = Null_cap;
    } else {
	if ((err = my_lookup(bullet_name[1], &bullet_caps[1])) != STD_OK) {
	    panic3("cannot lookup bullet 1 capability \"%s\" (%s)",
		   bullet_name[1], err_why(err));
	}
    }

    /* bullet caps are stored at soap object 0 */
    set_super_caps(sp_blocks, ALLOC_BLOCKS, (objnum) 0, bullet_caps);

    /*
     * Get the vdisk capabilities (to store the super files)
     */
    if ((err = my_lookup(vdisk_name[0], &vdisk_caps[0])) != STD_OK) {
	panic3("cannot lookup vdisk 0 capability \"%s\" (%s)",
	       vdisk_name[0], err_why(err));
    }
    if (!add_member && !force) { /* check before doing anything drastic */
	check_superblock(vdisk_name[0], &vdisk_caps[0]);
    }

    if (!add_member && strcmp(vdisk_name[1], "-") == 0) {
	vdisk_caps[1] = Null_cap;
    } else {
	if ((err = my_lookup(vdisk_name[1], &vdisk_caps[1])) != STD_OK) {
	    panic3("cannot lookup vdisk 1 capability \"%s\" (%s)",
		   vdisk_name[1], err_why(err));
	}

	/* sanity check: vdisk servers must be different */
	if (memcmp(&vdisk_caps[0], &vdisk_caps[1], sizeof(capability)) == 0) {
	    panic3("virtual disk servers `%s' and `%s' are the same",
		   vdisk_name[0], vdisk_name[1]);
	}

	if (!force) {
	    /* check before doing anything drastic */
	    check_superblock(vdisk_name[1], &vdisk_caps[1]);
	}

	vdisk1_valid = 1;
    }

    /* Get the vdisk sizes.  In case the `nblks' argument is 0, use them
     * to determine the number of blocks to use.  Then check if it fits.
     */
    if ((err = disk_size(&vdisk_caps[0], L2VBLKSZ, &size_0)) != STD_OK) {
	panic3("cannot get size of %s (%s)", vdisk_name[0], err_why(err));
    }
    if (vdisk1_valid &&
	(err = disk_size(&vdisk_caps[1], L2VBLKSZ, &size_1)) != STD_OK)
    {
	panic3("cannot get size of %s (%s)", vdisk_name[1], err_why(err));
    }

    if (add_member) {
	int copy_mode;

	/* Get number of blocks (and other info) from existing member */
	err = get_member_info(&vdisk_caps[0], &nblks, &copy_mode,
			      &priv_getcaps[0], &pub_getcap);
	if (err != STD_OK) {
	    panic3("cannot scan superblock on %s (%s)",
		   vdisk_name[0], err_why(err));
	}

	/* We expect the existing member to be in one copy mode */
	if (copy_mode != 1) {
	    fprintf(stderr, "Warning: existing member in %d copy mode\n",
		    copy_mode);
	}
    } else {
	if (nblks == 0) {
	    /* Default size is the one that just fits on both of them */
	    nblks = size_0;
	    if (vdisk1_valid && (size_1 < nblks)) {
		nblks = size_1;
	    }
	    if (nblks > DEF_MAX_BLOCKS) { /* make it something reasonable */
		nblks = DEF_MAX_BLOCKS;
	    }

	    printf("Default size of superfile is %ld\n", (long) nblks);
	}
    }

    if ((nblks < MIN_BLOCKS) ||	(nblks > size_0) ||
	(vdisk1_valid && (nblks > size_1)))
    {
	fprintf(stderr, "Invalid number of super blocks: %ld\n", (long) nblks);
	fprintf(stderr, "%s has %ld blocks\n", vdisk_name[0], (long) size_0);
	if (vdisk1_valid) {
	    fprintf(stderr,
		        "%s has %ld blocks\n", vdisk_name[1], (long) size_1);
	}

	exit(3);
    }

    /*
     * Invent and store private and public server capabilities.
     */
    if (add_member) {
	capability own_putcap;
	
#ifdef SOAP_GROUP
	/* put server's own cap first, indepent of its member id */
	priv_getcaps[1] = priv_getcaps[0];
	makecap(&priv_getcaps[0], &own_putcap);
#else
	makecap(&priv_getcaps[1], &own_putcap);
#endif
	store_cs_cap(SUPER_CAP_NEW, &own_putcap);
	printf("New soap super capset stored in \"./%s\"\n", SUPER_CAP_NEW);
    } else {
	capability priv_putcaps[2];	/* soap private capabiliy (putport) */
	
	makecap(&priv_getcaps[0], &priv_putcaps[0]);
	store_cs_cap(SUPER_CAP0, &priv_putcaps[0]);
	printf("Soap 0 super capset stored in \"./%s\"\n", SUPER_CAP0);

	if (vdisk1_valid) {
	    makecap(&priv_getcaps[1], &priv_putcaps[1]);
	    store_cs_cap(SUPER_CAP1, &priv_putcaps[1]);
	    printf("Soap 1 super capset stored in \"./%s\"\n", SUPER_CAP1);
	} else {
	    priv_getcaps[1] = Null_cap;
	    priv_putcaps[1] = Null_cap;
	}

	makecap(&pub_getcap, &pub_putcap);
    }

    if (!add_member) {
	/* Create initial directory, and add bullet capabilities for it
	 * to the super file.
	 */
	capability file_caps[NBULLETS];

	create_dir(&dir_cap, (objnum) 1, bullet_caps, file_caps);
	dir_cap.cap_port = pub_putcap.cap_port;

	set_super_caps(sp_blocks, ALLOC_BLOCKS, (objnum) 1, file_caps);

#ifdef AMOEBA
	if ((err = name_append(root_name, &dir_cap)) != STD_OK) {
	    panic3("cannot install new root capability \"%s\" (%s)",
		   root_name, err_why(err));
	}
	printf("New Soap root published in \"%s\"\n", root_name);
#else
	{
	    char capbuf[sizeof(capability)];
	    char *end;

	    end = buf_put_cap(capbuf, capbuf + sizeof(capbuf), &dir_cap);
	    if (end == NULL) {
		panic("cannot put capability in buffer");
	    }

	    store_buf(root_name, 0400, capbuf, (int) (end - capbuf));
	    printf("Created new capability file \"%s\"\n", root_name);
	}
#endif
    }
				       
    /*
     * Fill in the super block.
     */
    Super_block.sb_public = pub_getcap;
    Super_block.sb_private.sc_caps[0] = priv_getcaps[0];
    Super_block.sb_private.sc_caps[1] = priv_getcaps[1];
    Super_block.sb_magic = SP_MAGIC;
#ifdef SOAP_DIR_SEQNR
    Super_block.sb_dirseq.seq[0] = 0;
    Super_block.sb_dirseq.seq[1] = 1;
    Super_block.sb_modoff = -1; /* no mod list by default */
#else
    Super_block.sb_super.sc_caps[0] = vdisk_caps[0]; /* not very useful */
    Super_block.sb_super.sc_caps[1] = vdisk_caps[1]; /* not very useful */
#endif
    Super_block.sb_seq = 0;		/* no operations performed yet */
    Super_block.sb_copymode = 1;
    Super_block.sb_nblocks = nblks;	/* total number of super blocks */
    Super_block.sb_nintent = 0;		/* no intentions */

    /*
     * Write super file 0.
     */
    if (!add_member) {
	printf("Writing super file #0...\n");
	err = write_super(&vdisk_caps[0], &Super_block,
			  sp_blocks, ALLOC_BLOCKS, nblks);
	if (err != STD_OK) {
	    panic3("cannot write super file #0 to \"%s\" (%s)",
		   vdisk_name[0], err_why(err));
	}
    }
	
    /*
     * Write super file 1.
     */
    if (vdisk1_valid) {
	/*
	 * Let the new Soap come up in two copy mode.
	 * In case of the Group Soap, the server itself will replace it
	 * with a suitable new-style copymode.
	 *
	 * If we are only adding a new member, the new server itself will
	 * take care of fetching the directory contents from one of the
	 * other members (because its sequence number is smaller).
	 */
	Super_block.sb_copymode = 2;

	printf("Writing super file #1...\n");
	err = write_super(&vdisk_caps[1], &Super_block,
			  sp_blocks, ALLOC_BLOCKS, nblks);
	if (err != STD_OK) {
	    panic3("cannot write super file #1 to \"%s\" (%s)",
		   vdisk_name[1], err_why(err));
	}
    }

    /*
     * Done.
     */
    printf("New Soap System ready.\n");
}

/*
 * Main program.
 */

extern char *optarg;
extern int   optind;
extern int   getopt();

static void
usage()
{
    fprintf(stderr,
	    "Usage: %s [options] bullet0 bullet1 vdisk0 vdisk1 [size]\n",
	    Prog_name);

    fprintf(stderr, "\nArguments:\n");
    fprintf(stderr, "bullet0\t: bullet super capability for Soap0\t%s\n",
	    "(e.g., \"host0/bullet\")");
    fprintf(stderr, "bullet1\t: bullet super capability for Soap1\t%s\n",
	    "(e.g., \"host1/bullet\")");
    fprintf(stderr, "vdisk0\t: vdisk capability for Soap0 super file\t%s\n",
	    "(e.g., \"host0/vdisk:xx\")");
    fprintf(stderr, "vdisk1\t: vdisk capability for Soap1 super file\t%s\n",
	    "(e.g., \"host1/vdisk:xx\")");
    fprintf(stderr, "size\t: number of blocks for super files\t%s\n",
	    "(e.g., 99)");

    fprintf(stderr, "\nbullet1 and vdisk1 may be \"-\" indicating none.\n");

    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "-f\t: overwrite existing Soap super file\n");
    fprintf(stderr,
	    "-A\t: add new member on vdisk1 (existing member on vdisk0)\n");
#ifdef AMOEBA
    fprintf(stderr,
	    "-c cap\t: publish soap root in `cap' (default \"%s\")\n",
	    DEF_NEW_ROOT);
#else
    fprintf(stderr,
	    "-c cap\t: put soap capability in `cap' (default \"$HOME/%s\")\n",
	    CAP_FILENAME);
#endif

#ifdef SOAP_GROUP
    fprintf(stderr, "\n\tor\n\n");

    fprintf(stderr, "Usage[2]: %s -M modlist_size vdisk\n", Prog_name);
#endif

    exit(2);
}

main(argc, argv)
int   argc;
char *argv[];
{
    char       *bullet_name[NBULLETS];
    char       *vdisk_name[NBULLETS];
    char       *root_name;
    int		modlist_size;
    int		opt;
    int		force;		/* flag to force overwriting old super file */
    int		add_member;

    if (argc >= 1) {
	Prog_name = argv[0];
    }

    /*
     * Get options and arguments.
     */
    root_name = NULL;	/* defaults are OS dependent;  see below */
    force = 0;		/* check that disk doesn't contain a Soap super file */
    modlist_size = 0;
    add_member = 0;

    while ((opt = getopt(argc, argv, "c:fAM:?")) != EOF) {
	switch (opt) {
	case 'c':
	    root_name = optarg;
	    break;
	case 'f':
	    force = 1;	/* explicitly overwrite previous Soap super file */
	    break;
	case 'A':
	    add_member = 1;
	    break;
#ifdef SOAP_GROUP
	case 'M':
	    modlist_size = atoi(optarg);
	    break;
#endif
	case '?':
	default :
	    usage();
	    break;
	}
    }

    if (modlist_size != 0) {
	/* Add modlist to existing partition */
	if (argc - optind != 1) {
	    usage();
	}

	vdisk_name[0] = argv[optind++];
	add_modlist(vdisk_name[0], (disk_addr) modlist_size);
    } else {
	disk_addr nblks;

	/* There should be at least 4 extra arguments. */
	if (argc - optind < 4) {
	    usage();
	}
	    
	bullet_name[0] = argv[optind++];
	bullet_name[1] = argv[optind++];
	vdisk_name[0] = argv[optind++];
	vdisk_name[1] = argv[optind++];
    
	/* handle remaining size argument, if any */
	switch (argc - optind) {
	case 0:
	    /* if not specified, take the whole vdisk (or the same as the
	     * existing member if we're adding one).
	     */
	    nblks = 0;
	    break;
	case 1:
	    if (add_member) {
		fprintf(stderr, "size argument ignored with -A\n");
		nblks = 0;
	    } else {
		nblks = atoi(argv[optind++]);
		if (nblks < MIN_BLOCKS) {
		    panic("argument nblocks must be at least 3");
		}
	    }
	    break;
	default:
	    usage();
	    break;
	}

	create_superfiles(force, add_member, nblks,
			  bullet_name, vdisk_name, root_name);
    }

    exit(0);
}
