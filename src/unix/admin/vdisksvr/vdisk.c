/*	@(#)vdisk.c	1.4	96/02/27 11:57:49 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "server/disk/disk.h"
#ifndef JUSTVDISK
#include "soap/super.h"
#endif
#include "module/prv.h"
#include "module/name.h"
#include "monitor.h"

#include "signal.h"
#include "stdlib.h"

#define SIGAMOEBA SIGEMT

static errstat	disk_rw();
static errstat	disk_size();
static errstat	disk_std_restrict();


/* largest and smallest acceptable value for log2(virtual block size) */
#define MAX_VBLK_SHIFT          31
#define MIN_VBLK_SHIFT          D_PHYS_SHIFT

disk_addr v_numblks;
int super;

/* port listened to by disk server */
static	port	Diskport, Diskrand;

/*
** DISK_TASK
**
**	The disk task provides the interface to tasks running in user space.
**	They communicate with the disk server via transactions and all data
**	is transferred via this interface.  No direct memory mapping is
**	supported.  (Kernel tasks call the disk_rw() routine directly.)
**
**  NB:  Because of Amoeba 3's headers we do some special manipulations to
**	 protect against sign extension.  These little gremlims should come
**	 out for AMOEBA4.
*/

disk_task()
{
    header	hdr;
    bufptr	buf;		/* for read/write of data */
    bufptr	replybuf;	/* pointer to reply buffer */
    bufsize	n;		/* return value of getreq */
    int		l2vblksz;	/* virtual block size in bytes */
    disk_addr	numblks;	/* # blocks to read/write */
    char	info_str;	/* character containing STD_INFO data */

/* get a buffer to do the transactions with */
    if ((buf = (bufptr) malloc(D_REQBUFSZ)) == 0)
	panic("disk_task: Cannot allocate transaction buffer.");

    info_str = '@';
/* listen for and execute requests */
    for(;;)
    {
	hdr.h_port = Diskport;	/* initialised at boot time by disk_init() */
	replybuf = NILBUF;	/* always true except for READ */
	n = getreq(&hdr, buf, D_REQBUFSZ);
	if (ERR_STATUS(n))
	{
	    printf("disk_task: getreq failed: %d.\n", n);
	    panic("");
	}

	l2vblksz = (short) hdr.h_size; /*AMOEBA4*/
    /*
    ** for Amoeba 3 we are stuck with small headers so the number of blocks
    ** per read is restricted.
    ** for AMOEBA4:
	numblks = hdr.h_extra;
    */
	numblks = (long) (hdr.h_extra & 0xffff);  /* stop sign extension */

    /*
    ** Capabilities are checked in the interface routines.
    ** Disk resources are locked with a mutex within the routines
    ** to prevent pathogenic interactions.
    */
	switch (hdr.h_command)
	{
	case STD_AGE:
	case STD_TOUCH:
	    hdr.h_size = 0;
	    hdr.h_status = STD_OK;
	    break;

	case STD_INFO:
	    replybuf = &info_str;
	    hdr.h_size = 1;
	    hdr.h_status = STD_OK;
	    break;

	case STD_RESTRICT:
	    hdr.h_status = disk_std_restrict(&hdr.h_priv, (rights_bits) hdr.h_offset);
	    hdr.h_size = 0;
	    break;

	case DS_READ:
	    hdr.h_status = disk_rw(DS_READ, &hdr.h_priv, l2vblksz, hdr.h_offset, numblks, buf);
	    if (ERR_STATUS(hdr.h_status))
		hdr.h_size = 0;
	    else
		hdr.h_size = numblks << l2vblksz;
	    replybuf = buf;
	    break;

	case DS_SIZE:
	    hdr.h_status = disk_size(&hdr.h_priv, l2vblksz, &hdr.h_offset);
	    hdr.h_size = 0;
	    break;

	case DS_WRITE:
	    hdr.h_status = disk_rw(DS_WRITE, &hdr.h_priv, l2vblksz, hdr.h_offset, numblks, buf);
	    hdr.h_size = 0;
	    break;

/*
	case STD_COPY:
	case STD_DESTROY:
*/
	default:
	    hdr.h_status = STD_COMBAD;
	    hdr.h_size = 0;
	    break;
	}
	putrep(&hdr, replybuf, hdr.h_size);
    }
    /*NOTREACHED*/
}


/*
** DISK_RW
**
**	Returns STD_OK if it could successfully read/write
**	"vblksz" * "num_vblks" bytes beginning at disk block "start" from/to
**	the virtual disk specified by "priv" into/from "buf".  Otherwise it
**	returns an error status indicating the nature of the fault.
**
**  NB: This routine can be called directly by kernel servers accessing a disk.
*/

static errstat
disk_rw(cmd, priv, l2vblksz, vstartblk, num_vblks, buf)
int		cmd;		/* selects read or write */
private *	priv;		/* capability for the virtual disk */
int		l2vblksz;	/* log2(virtual block size) */
disk_addr	vstartblk;	/* first block to read */
disk_addr	num_vblks;	/* number of virtual blocks to read */
bufptr		buf;		/* place to read into */
{
    register disk_addr	num_pblks;	/* number of physical blocks to read */
    register disk_addr	pstartblk;	/* physical starting block */
    int		size;		/* # physical bytes to r/w from partition */
    rights_bits	rights;		/* to check write permission */

/* Validate capability */
    if (prv_number(priv) != 0 ||
	    prv_decode(priv, &rights, &Diskrand) < 0 ||
	    (cmd == DS_WRITE && !(rights & RGT_WRITE)))
	return STD_CAPBAD;

/*
** Convert virtual blocks to physical blocks.
** With virtual blocks smaller than physical blocksize we calculate the start
** physical block # and then for the number of blocks to read/write we
** calculate "last physical block - start physical block + 1".
*/
/* for now we have set MIN_VBLK_SHIFT to D_PHYS_SHIFT so we can get on with it.
    if (l2vblksz < D_PHYS_SHIFT)
    {
	register disk_addr scale;

	scale = D_PHYS_SHIFT - l2vblksz ;
	pstartblk = vstartblk >> scale;
	num_pblks = ((vstartblk + num_vblks) >> scale) - pstartblk;
    }
    else
*/
    {
	register disk_addr scale;

	scale = l2vblksz - D_PHYS_SHIFT;
	pstartblk = vstartblk << scale;
	num_pblks = num_vblks << scale;
    }

/* Some safety checks against bad parameters */
    if (buf == 0 || num_vblks <= 0 ||
		l2vblksz < MIN_VBLK_SHIFT || l2vblksz > MAX_VBLK_SHIFT ||
		pstartblk < 0 || pstartblk > v_numblks ||
		pstartblk + num_pblks > v_numblks)
	return STD_ARGBAD;

/*
** pstartblk now points to the start offset within partition "partn".
**  Make no effort to handle problems due to l2vblksz < D_PHYS_SHIFT.
*/
    lseek(super, (long) pstartblk << D_PHYS_SHIFT, 0);
    size = (int) num_pblks << D_PHYS_SHIFT;
    switch (cmd)
    {
    case DS_READ:
	if (read(super, buf, size) != size)
	    panic("can't read from unix vdisk file");
	break;
    case DS_WRITE:
	if (write(super, buf, size) != size)
	    panic("can't write to unix vdisk file");
	sync();
	break;
    default:
	panic("invalid disk command\n");
    }
    return STD_OK;
}


/*
** DISK_SIZE
**
**	Returns STD_CAPBAD if the capability was invalid or referred to a
**	virtual disk with size <= 0.  Otherwise it returns STD_OK and
**	returns in "maxblocks" the maximum number of virtual blocks of size
**	2^"l2vblksz" that fit on the virtual disk specified by "priv".  
**
**  NB: This routine can be called directly by kernel servers accessing a disk.
*/

static errstat
disk_size(priv, l2vblksz, maxblocks)
private *	priv;		/* capability for virtual disk */
int		l2vblksz;	/* log2(virtual block size) */
disk_addr *	maxblocks;	/* returned: max # blocks that fit on disk */
{
    register int	conv_factor;	/* conversion factor */
    rights_bits		rights;		/* dummy variable for prv_decode() */

/* validate capability */
    if (prv_number(priv) != 0 ||
	    prv_decode(priv, &rights, &Diskrand) < 0)
	return STD_CAPBAD;

/* make sure log2(virtual block size) is sensible */
    if (l2vblksz < MIN_VBLK_SHIFT || l2vblksz > MAX_VBLK_SHIFT)
	return STD_ARGBAD;
	
/* Convert physical blocks to virtual blocks, subtract # disk labels */
    if ((conv_factor = l2vblksz - D_PHYS_SHIFT) < 0)
	*maxblocks = v_numblks << -conv_factor;
    else
	*maxblocks = v_numblks >> conv_factor;
    return STD_OK;
}


/*
** The following implements the STD_RESTRICT command
** There is only one rights bit anyway so it isn't too exciting.
*/

static errstat
disk_std_restrict(priv, rights_mask)
private *	priv;		/* private part of cap for virtual disk */
rights_bits	rights_mask;	/* rights to be retained in new cap */
{
    rights_bits	rights;

/* validate capability */
    if (prv_number(priv) != 0 ||
	    prv_decode(priv, &rights, &Diskrand) < 0)
	return STD_CAPBAD;

    rights &= rights_mask;

    if (prv_encode(priv, 0L, rights, &Diskrand) != STD_OK)
	return STD_CAPBAD;
    return STD_OK;
}

panic(s)
char *s;
{
	printf("unix vdisk: %s\n", s);
	exit(1);
}

#ifndef JUSTVDISK
main(argc, argv)
char **argv;
{
	static char file[8] = "super";
	union sp_block commit;

	if (argc != 2 || argv[1][1]!=0)
		panic("Usage: super [01]");
	strcat(file, argv[1]);
	if ((super = open(file, 2)) < 0)
		panic("can't open super file");
	if (read(super, commit.sb_block, BLOCKSIZE) != BLOCKSIZE)
		panic("can't read super commit");
	Diskport = commit.sb_super.sc_caps[atoi(argv[1])].cap_port;
	Diskrand = commit.sb_super.sc_caps[atoi(argv[1])].cap_priv.prv_random;
	v_numblks = (commit.sb_nblocks * BLOCKSIZE) >> D_PHYS_SHIFT;
	signal(SIGAMOEBA, SIG_IGN);
	disk_task();
	/*NOTREACHED*/
}
#else /* JUSTVDISK */

#include <sys/types.h>
#include <sys/stat.h>

main(argc, argv)
char **argv;
{
	capability mycap;
	struct stat stbuf;

	if (argc != 3)
		panic("Usage: vdisk file cap");
	if (name_lookup(argv[2], &mycap) != STD_OK)
		panic("Capability not found");
	Diskport = mycap.cap_port;
	Diskrand = mycap.cap_priv.prv_random;
	if ((super = open(argv[1], 2)) < 0)
		panic("can't open super file");
	if (fstat(super, &stbuf) !=0)
		panic("can't stat super file");
	v_numblks = stbuf.st_size >>D_PHYS_SHIFT;
	signal(SIGAMOEBA, SIG_IGN);
	disk_task();
	/*NOTREACHED*/
}
#endif /* JUSTVDISK */
