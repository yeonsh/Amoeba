/*	@(#)virdisk.c	1.13	96/02/27 14:13:34 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	AMOEBA DISK THREAD
**
** This is the standard Amoeba disk thread.  It provides a virtual disk
** interface with user selectable virtual block size.  Virtual disks can span
** partition boundaries and physical disks.
** The final crowning glory is that it can share a physical disk with another
** operating system that supports disk partitioning.  The disk thread works out
** which partition(s) are for its use and leaves the others alone.
**
**  NB:
**   1. This is the server for user threads.  Servers within the kernel can
**	call the server's subroutines directly without going via trans()
**	Mutex semaphores prevent pathogenic interactions.
**   2. The server can be multi-threaded.
**
** Author:
**	Greg Sharp 07-02-89
*/

#include <string.h>
#include "amoeba.h"
#include "assert.h"
INIT_ASSERT
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "machdep.h" /* for vir_bytes */
#include "disk/disk.h"
#include "vdisk/disklabel.h"
#include "vdisk/disktype.h"
#include "vdisk/vdisk.h"
#include "disk/conf.h"
#include "module/prv.h"
#include "module/rnd.h"
#include "module/mutex.h"
#include "module/buffers.h"
#include "sys/proto.h"

#define	MIN(a, b)	((a) < (b) ? (a) : (b))

/* port listened to by disk server - initialised in disk_init() */
static	port	Diskport;

/* mutual exclusion semaphore */
static	mutex	Disk_sema;

/* Head of array of pointers to virtual disks which is allocated later */
static	vdisktab **	Vdisktab;
static	objnum		Num_virtual_disks;

/* Linked list of physical partitions assigned to Amoeba */
static	part_list *	Physparttab;

/* Used to ensure initialisation is done only once */
static	int		Initialised;

/* Pointer to aalloc'ed buffer to hold data for disk_info command */
static	char *		Dk_info_buf;

/* Column masks for floppy drive capabilities */
#define	FL_NMSKS	3
static	long		Fl_cols[] = { 0xFF, 0x6, 0x6 };

/* initialised in diskconf.c */
extern	ctlrtype	Cntlrtab[];
extern	confrec		Conf_tab[];

extern uint16		checksum _ARGS(( uint16 * p ));

/* forward declaration of functions */
int			am_partn_label();
int			host_getlabel();
disk_addr		host_partn_offset();
void			mk_fake_partn();
disk_addr		total_blocks();


/*
** CAPMATCH
**
**	Looks through the list of currently known virtual disks for one with 
**	the same capability as 'cap'.  If none is found it returns 0.
**	Otherwise it returns a pointer to the vdisk.
*/

static vdisktab *
capmatch(cap, vdlist)
capability *	cap;
vdisktab *	vdlist;
{
    objnum	capnum;		/* object number in capability */
    vdisktab *	vdp;		/* pointer to current virtual disk */

    capnum = prv_number(&cap->cap_priv);
    for (vdp = vdlist; vdp != 0; vdp = vdp->v_next)
	if (prv_number(&vdp->v_cap.cap_priv) == capnum)
	    break;  /* found a disk with same object number */
    return vdp;
}


/*
**  ADD_PARTN
**	add a partition to the set of known amoeba partitions
*/

static void
add_partn(vdlist, ctl_type, ctl_addr, unit, firstblk, plabel, amlabel)
vdisktab **	vdlist;		/* address of pointer to list of virt. disks */
int		ctl_type;	/* type of disk controller */
long		ctl_addr;	/* device address of disk */
int		unit;		/* unit # on device */
disk_addr	firstblk;	/* first physical block of partition */
disklabel *	plabel;		/* amoeba label on the partition */
disklabel *	amlabel;	/* buffer containing disk label */
{
    vpart *	ip;		/* pointer to virtual disk partition table */
    int		j;		/* sub-partition loop counter */
    part_list *	plp;		/* pointer to new partition list struct */
    part_tab *	spp;		/* pointer to sub-partition table of label */
    vdisktab *	vdp;		/* pointer to new vdisk struct */
    vdisktab *	edp;		/* pointer to end of vdisk list */

/* An Amoeba partition! Allocate a part_list struct for it. */
    if ((plp = (part_list *) aalloc((vir_bytes) sizeof (part_list), 0)) == 0)
	panic("disk thread: cannot alloc part_list struct.");

/* Put it on front of linked list and fill it in */
    plp->next = Physparttab;
    Physparttab = plp;
    plp->ctlr.type = ctl_type;
    plp->ctlr.address = ctl_addr;
    plp->ctlr.unit = unit;
    plp->labelblock = firstblk;
    plp->label = *plabel;

/* Find end of vdisk list */
    if (*vdlist == 0)
	edp = 0;
    else
	for (edp = *vdlist; edp->v_next != 0; edp = edp->v_next)
	    /* do nothing */;

/* For each sub-partition work out its virtual disk */
    for (spp = plabel->d_ptab, j = 0; j < MAX_PARTNS_PDISK; spp++, j++)
    {
	if (spp->p_numblks <= 0)	/* ignore empties */
	    continue;
	if ((vdp = capmatch(&spp->p_cap, *vdlist)) == 0)
	{
	    objnum	vdnum;	/* virtual disk number */

	/* Record largest vdisknum */
	    if ((vdnum = prv_number(&spp->p_cap.cap_priv)) > Num_virtual_disks)
		Num_virtual_disks = vdnum;

	/* Make a node & add to the end of linked list */
	    if ((vdp = (vdisktab *) aalloc(
				    (vir_bytes) sizeof (vdisktab), 0)) == 0)
		panic("disk_init/add_partn: aalloc for vdisk failed");
	    vdp->v_next = (vdisktab *) 0;
	    if (edp == 0)
		*vdlist = edp = vdp;
	    else
	    {
		edp->v_next = vdp;
		edp = vdp;
	    }

	/* set up virtual disk */
	    vdp->v_many = spp->p_many;
	    vdp->v_cap = spp->p_cap;
	    for (ip = vdp->v_parts; ip < &vdp->v_parts[MAX_PARTNS_VDISK]; ip++)
		ip->i_physpart = 0;
	    vdp->v_numblks = 0;
	    
	}
	else
	{
	    if (!PORTCMP(&spp->p_cap.cap_priv.prv_random,
			&vdp->v_cap.cap_priv.prv_random)
		    || !PORTCMP(&spp->p_cap.cap_port, &vdp->v_cap.cap_port))
	    {
		printf("disk_init/add_partn: invalid vdisk capability\n");
		continue;
	    }
	    /* Check v_many matches the value in the table */
	    if (spp->p_many != vdp->v_many)
	    {
		printf("disk_init/add_partn: inconsistent virtual disk partition\n");
		continue;
	    }
	    /* Check we haven't had this partition # already */
	    if (vdp->v_parts[spp->p_piece].i_physpart != 0)
	    {
		printf("disk_init/add_partn: two partitions for same slot\n");
		continue;
	    }
	}
    /* The +1's & -1's below are to step over partn label */
	vdp->v_parts[spp->p_piece].i_physpart = plp;
	vdp->v_parts[spp->p_piece].i_firstblk = spp->p_firstblk + 1
						+ host_partn_offset(amlabel);
	vdp->v_parts[spp->p_piece].i_numblks = spp->p_numblks - 1;
	vdp->v_numblks += spp->p_numblks - 1;
    }
}


/*
** AM_GETLABEL
**
**	This routine reads an Amoeba disk label from block 0 and puts the
**	partition and geometry info into an amoeba label.  Returns -1 if
**	the read failed.  Returns 1 if the label was invalid.
**	Returns 0 on success.
*/

static int
am_getlabel(dk_read, devaddr, unit, am_label)
errstat		(*dk_read)();	/* read routine for disk */
long		devaddr;	/* device address of disk controller */
int		unit;		/* unit number of disk to read from */
disklabel *	am_label;	/* amoeba label */
{

/* Read the label */
    if ((*dk_read)(devaddr, unit, 0, 1, am_label) != STD_OK)
	return -1;
    return am_partn_label(am_label);
}


/*
** DISK_INIT
**
**	This routine goes through all the available disks and finds the
**	disks and/or partitions which are for use by Amoeba and builds up the
**	tables of virtual disks and devices.  It also makes a bootp or floppy
**	partition which gives access to the complete physical unit.
**	The following scheme is used:
**
**	for (each disk controller in the config table)
**	{
**	    if (we can't initialise the device)
**		skip to next device.
**	    for (each unit on the device)
**	    {
**		if (we can't initialise the unit)
**		    skip to next unit.
**		if (it is a fixed disk)
**		{
**		    read block 0.
**		    if (block 0 is an amoeba label)
**			add it as a single partition.
**		    else
**		    {
**			read the host label.
**			if (no label)
**			    skip to next unit.
**			for (each partition)
**			    if (partition has amoeba label)
**			    {
**				put it on list of physical disk partitions.
**				work out which virtual disk it belongs to and
**				put it on list of virtual disks.
**			    }
**		    }
**		    make a bootp fake partition for the "physical unit"
**		}
**		else
**		    if (it is a removable disk)
**			make a floppy fake partition for the "physical unit"
**	    }
**	}
**	make a table of pointers to virtual disks so we can access via
**	virtual disk number.
*/

static void
disk_init()
{
    static capability	cap;	/* used to register disk caps -static so it
				** is initialised to the null port */

    disklabel	amlabel;	/* buffer for amoeba disk label equivalent */
    int		max_phys_parts;	/* max # phys. partitions for any vdisk */
    confrec *	cp;		/* pointer to disk configuration table */
    ctlrtype *	dcp;		/* pointer to controller type entry */
    objnum	dknum;		/* number of phys disk in vdisktab */
    int		error;		/* flag */
    disk_addr	firstblk;	/* address of first block of phys partition */
    int		i;		/* phys partition loop counter */
    objnum	num_fdisks;	/* number of floppy disks for which we have
				   made a virtual disk entry */
    objnum	num_pdisks;	/* count of # physical disks for which we have
				   made fake virtual disks */
    vdisktab *	fdlist;		/* pointer to head of list of floppy disks */
    vdisktab *	pdlist;		/* pointer to head of list of physical disks */
    disklabel	plabel;		/* partition label */
    part_tab *	ptp;		/* pointer to partition table of label */
    int		unit;		/* current disk unit number */
    vdisktab *	vdp;		/* pointer to vdisk struct */
    vdisktab *	vdlist;		/* pointer to head of list of virtual disks */
    objnum	vdisknum;	/* virtual disk number */
    int		unittype;	/* current disk type */

    vdlist = 0;		/* list of virtual disks, initially empty */
    fdlist = 0;		/* list of floppy disks, initially empty */
    pdlist = 0;		/* list of physical disks, initially empty */
    num_fdisks = 0;
    num_pdisks = 0;
    for (cp = Conf_tab; cp->c_maxunit > cp->c_minunit; cp++)
    {
    /* initialise the controller device */
	assert(cp->c_ctlr_type >= 0);
	dcp = &Cntlrtab[cp->c_ctlr_type];
	assert(dcp != 0);
	assert(dcp->c_devinit != 0);

	if ((*dcp->c_devinit)(cp->c_dev_addr, cp->c_ivec) != STD_OK)
	    continue;

    /* look at all units for now!  later conftabs can specify unit numbers */
	for (unit = cp->c_minunit; unit < cp->c_maxunit; unit++)
	{
	/* initialise the unit */
	    if ((*dcp->c_unitinit)(cp->c_dev_addr, unit) != STD_OK)
	    {
		(void) await((event) 0, (interval) 1); /* let interrupts run */
		continue;
	    }

	/* determine unit type */
	    unittype = (*dcp->c_unittype)(cp->c_dev_addr, unit);

	/*
	** If fixed disk then try to get the label.  See if it is an amoeba
	** label at sector 0.  If not then see if it is a host os label
	*/
	    if (unittype == UNITTYPE_FIXED)
	    {
		if (am_getlabel(dcp->c_read, cp->c_dev_addr, unit, &amlabel) > 0)
		{
		    firstblk = amlabel.d_ptab->p_firstblk +
						host_partn_offset(&amlabel);
		    add_partn(&vdlist, cp->c_ctlr_type, cp->c_dev_addr,
					    unit, firstblk, &amlabel, &amlabel);
		}
		else
		{
		    if (host_getlabel(dcp->c_read, cp->c_dev_addr, unit,
						    (char *) 0, &amlabel) == 0)
		    {
		    /* now look at each partition to see if it is for Amoeba */
			for (ptp = amlabel.d_ptab, i = 0; i < MAX_PARTNS_PDISK; ptp++, i++)
			{
			/* skip empty partitions */
			    if (ptp->p_numblks == 0)
				continue;

			/* read block 0 of the partition */
			    firstblk = ptp->p_firstblk +
						host_partn_offset(&amlabel);
			    if ((*dcp->c_read)(cp->c_dev_addr, unit, firstblk,
						1, (char *) &plabel) != STD_OK)
			    {
				printf("read failed for %s disk at %lx, unit %d, block %ld\n",
				   dcp->c_name, cp->c_dev_addr, unit, firstblk);
				break;	/* read failure!  go to next UNIT */
			    }

			/*
			** Is it an Amoeba label?
			** Watch out for overlapping partitions.
			*/
			    if (am_partn_label(&plabel) &&
				  ptp->p_numblks == total_blocks(plabel.d_ptab))
			    {
				add_partn(&vdlist, cp->c_ctlr_type,
						cp->c_dev_addr, unit, firstblk,
						&plabel, &amlabel);
			    }
			}
		    }
		    else
		    {
		    /*
		    ** There is no label on the disk.  We have to make a bootp
		    ** for it anyway since the user mode disk label utility
		    ** needs some way of getting at the disk to write a label.
		    ** Therefore in amlabel we put enough dummy geometry info
		    ** for mk_fake_partn and add_partn to work
		    */
			printf("Vdisk WARNING: disk bootp:%02d has no label!\n",
								    num_pdisks);
			amlabel.d_geom.g_bpt = 0;
			amlabel.d_geom.g_bps = 0;
			amlabel.d_geom.g_numcyl = 0;
			amlabel.d_geom.g_altcyl = 0;
			amlabel.d_geom.g_numhead = 0;
			amlabel.d_geom.g_numsect = 0;
		    }
		}
	    /*
	    **  For each unit (physical disk) we also need access to the
	    **  boot partition which is not listed in the partition table.
	    **  Therefore we generate a fake label which says that the label
	    **  is at disk block -1.  This lets us access physical block 0
	    **  of the physical disk, which is needed to write boot blocks
	    **  and corrupt disk systems :^).
	    */
		mk_fake_partn(&plabel, &amlabel, num_pdisks, cp, unit);
		num_pdisks++;
		add_partn(&pdlist, cp->c_ctlr_type, cp->c_dev_addr, unit,
					    (disk_addr) -1, &plabel, &amlabel);
	    }
	    else
	    /* if removable disk then just publish its raw capability (ala bootp) */
		if (unittype == UNITTYPE_REMOVABLE)
		{
		    mk_fake_partn(&plabel, &amlabel, num_fdisks, cp, unit);
		    num_fdisks++;
		    add_partn(&fdlist, cp->c_ctlr_type, cp->c_dev_addr, unit,
					    (disk_addr) -1, &plabel, &amlabel);
		}

	    (void) await((event) 0, (interval) 1); /* let interrupts run */
	}
    }

/*
** Check to see if there were any disks.  We print a warning and go to sleep
** if there weren't any.  Since we don't unlock Disk_sema the all threads
** will block at initialisation so no disk server threads will run.
** The easiest way to sleep is to try to lock a mutex we have already locked.
*/
    if (num_pdisks == 0 && num_fdisks == 0)
    {
	printf("Virtual Disk Server: WARNING. No disks found on this system\n");
	mu_lock(&Disk_sema);
    }

/*
** Now we need to process the fake virtual disks so that they get a valid
** virtual disk number and a name in the kernel directory that shows that
** they are giving raw access to the partition.  We do this by giving them
** object numbers higher than any existing virtual disks and then processing
** the pdlist similarly to the vdlist.
*/
    dknum = Num_virtual_disks+1;
    for (vdp = pdlist; vdp != 0; vdp = vdp->v_next)
    {
	if (prv_encode(&vdp->v_cap.cap_priv, dknum, PRV_ALL_RIGHTS,
					&vdp->v_cap.cap_priv.prv_random) < 0)
	    panic("disk_init: prv_encode failed\n");
	dknum++;
    }

/*
** The same trick as above is also applied to removable disks.
** Eventually we would like to attach names like floppy:NN to
** these capabilities to prevent any confusion with bootp.
*/
    for (vdp = fdlist; vdp != 0; vdp = vdp->v_next)
    {
	if (prv_encode(&vdp->v_cap.cap_priv, dknum, PRV_ALL_RIGHTS,
					&vdp->v_cap.cap_priv.prv_random) < 0)
	    panic("disk_init: prv_encode failed\n");
	dknum++;
    }

/* 
** In case there are no virtual disks available,
** get a port for the disk server.
*/
    if (pdlist != 0 && vdlist == 0)
    {
	uniqport(&pdlist->v_cap.cap_port);
	for (vdp = pdlist->v_next; vdp; vdp = vdp->v_next)
	   vdp->v_cap.cap_port = pdlist->v_cap.cap_port;
    }
    if (fdlist != 0 && pdlist == 0 && vdlist == 0)
    {
	uniqport(&fdlist->v_cap.cap_port);
	for (vdp = fdlist->v_next; vdp; vdp = vdp->v_next)
	   vdp->v_cap.cap_port = fdlist->v_cap.cap_port;
    }

/* Join pdlist to vdlist to simplify processing */
    if (pdlist != 0)
    {
	for (vdp = pdlist; vdp->v_next != 0; vdp = vdp->v_next)
	    /* do nothing */;
	vdp->v_next = vdlist;
	vdlist = pdlist;
    }

/* Join fdlist to vdlist to simplyfy processing */
    if (fdlist != 0)
    {
	for (vdp = fdlist; vdp->v_next != 0; vdp = vdp->v_next)
	    /* do nothing */;
	vdp->v_next = vdlist;
	vdlist = fdlist;
    }


/* Allocate a table of pointers to vdisks */
    assert(dknum != 0);
    Vdisktab = (vdisktab **) aalloc(
				(vir_bytes) sizeof (vdisktab *) * dknum, 0);
    if (Vdisktab == 0)
	panic("disk_init: aalloc failed for Vdisktab");

/*
** Process the vdlist and find the capabilities for the ram & phys disks.
*/
    {
	port diskport;

	(void) memset((_VOIDSTAR) &diskport, 0, sizeof(port));
    	for (vdp = vdlist; vdp != 0; vdp = vdp->v_next)
	    if (!NULLPORT(&vdp->v_cap.cap_port))
	    {
		diskport = vdp->v_cap.cap_port;
		break;
	    }
	/* make sure everyone has the same (non-NULL) port */
	if (NULLPORT(&diskport))
	    uniqport(&diskport);
	for (vdp = vdlist; vdp != 0; vdp = vdp->v_next)
	    vdp->v_cap.cap_port = diskport;
	
	/* later when we publish the disk capabilities we need the put port */
	priv2pub(&diskport, &cap.cap_port);
    }
    max_phys_parts = 0;
    for (vdp = vdlist; vdp != 0; vdp = vdp->v_next)
    {
	error = 0;
	vdisknum = prv_number(&vdp->v_cap.cap_priv);
/* Remember what the maximum # partitions are for any virtual disk */
	if (vdp->v_many > max_phys_parts)
		max_phys_parts = vdp->v_many;
/* Should check that no pieces are missing! */
	for (i = 0; i < vdp->v_many; i++)
	    if (vdp->v_parts[i].i_physpart == 0)
	    {
		printf("Piece %d missing from virtual disk %d\n", i, vdisknum);
		error = 1;
	    }
	if (error == 0)	/* if disk is complete add it else leave it out! */
	{
	/*
	** Encrypt the capability and install it.
	** Note that the port was filled in earlier.
	*/
	    Vdisktab[vdisknum] = vdp;
	    if (prv_encode(&cap.cap_priv, vdisknum, PRV_ALL_RIGHTS,
					&vdp->v_cap.cap_priv.prv_random) < 0)
		panic("prv_encode failed");
	    if (vdisknum > Num_virtual_disks + num_pdisks)
	    {
		int dn;

		dn = vdisknum - (Num_virtual_disks + num_pdisks + 1);
		dirnappend(FLDISK_SVR_NAME, dn, &cap);
		dirnchmod(FLDISK_SVR_NAME, dn, FL_NMSKS, Fl_cols);
	    }
	    else
		if (vdisknum > Num_virtual_disks)
		    dirnappend(BPDISK_SVR_NAME,
				(int) (vdisknum - Num_virtual_disks - 1), &cap);
		else
		    dirnappend(VDISK_SVR_NAME, (int) vdisknum, &cap);
	}
    }

/*
** Alloc a buffer for the largest possible disk_info request.
** That consists of two disk addresses for each physical partition.
*/
    assert(max_phys_parts != 0);
    Dk_info_buf = aalloc((vir_bytes) (max_phys_parts * DISKINFOSZ), 0);
    if (Dk_info_buf == 0)
	panic("disk_init: aalloc failed for disk info buffer");


/* Now we need Num_virtual_disks to hold the true maximum vdisk number */
    Num_virtual_disks = dknum;

/* It must have a port to listen to.  Assume all vdisks have same port */
    if (vdlist != 0)
	Diskport = Vdisktab[vdisknum]->v_cap.cap_port;
    else
    {
	uniqport(&Diskport);    /* otherwise the getreq fails in main loop */
	printf("Disk_init: No virtual disks found on system.\n");
    }
}


/*
** DISK_INFO
**
**	This routine returns in 'buf' a pointer to an array containing 
**	containing the disk partition startblock and size for each
**	physical disk partition comprising a virtual disk.  The information
**	was calculated at boot time and stored in network byte order at
**	initialisation time.
*/

errstat
disk_info(hdr, buf)
header *	hdr;
bufptr *	buf;
{
    char *	e;		/* pointer to end of info bufer */
    int		i;		/* loop counter */
    char *	p;		/* pointer to start of info bufer */
    rights_bits	rights;
    objnum	vdisk;		/* number of virtual disk */
    vdisktab *	vdp;

/* Validate capability */
    if ((vdisk = prv_number(&hdr->h_priv)) < 0 || vdisk > Num_virtual_disks ||
	    (vdp = Vdisktab[vdisk]) == 0 ||
	    prv_decode(&hdr->h_priv, &rights,
				    &vdp->v_cap.cap_priv.prv_random) < 0)
	return STD_CAPBAD;
    if (!(rights & RGT_READ))
	return STD_DENIED;

/* copy the info into the buffer */
    p = Dk_info_buf;
    e = Dk_info_buf + vdp->v_many * DISKINFOSZ;
    for (i = 0; i < vdp->v_many; i++)
    {
	p = buf_put_int32(p, e, (int32) vdp->v_parts[i].i_physpart->ctlr.unit);
	p = buf_put_disk_addr(p, e, vdp->v_parts[i].i_firstblk);
	p = buf_put_disk_addr(p, e, vdp->v_parts[i].i_numblks);
    }

    if (p == 0)
	panic("disk_info: couldn't fit info data into buffer");

/* make replybuf point to the static disk info */
    *buf = Dk_info_buf;
    hdr->h_size = p - Dk_info_buf;
    return STD_OK;
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

errstat
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
    objnum	vdisk;		/* number of virtual disk */
    disk_addr	size;		/* # physical blocks to r/w from partition */
    vpart *	pp;		/* pointer to partition */
    errstat	stat;		/* result of operation */
    int		partn;		/* partition counter */
    rights_bits	rights;		/* to check write permission */
    errstat	(*iofunc)();	/* pointer to correct io function */
    vdisktab *	vdp;		/* pointer to virtual disk */
    contlr *	physdev;	/* pointer to physical disk controller */

/* Validate capability */
    if ((vdisk = prv_number(priv)) < 0 || vdisk > Num_virtual_disks ||
	    (vdp = Vdisktab[vdisk]) == 0 ||
	    prv_decode(priv, &rights, &vdp->v_cap.cap_priv.prv_random) < 0)
	return STD_CAPBAD;
    if ((cmd == DS_WRITE && !(rights & RGT_WRITE)) ||
				    (cmd == DS_READ && !(rights & RGT_READ)))
	return STD_DENIED;

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
		pstartblk < 0 || pstartblk > vdp->v_numblks ||
		pstartblk + num_pblks > vdp->v_numblks)
	return STD_ARGBAD;

/* Lock out other disk accesses */
    mu_lock(&Disk_sema);

/* Find partition where i/o starts.  We've already checked for bad args! */
    for (pp = vdp->v_parts, partn = 0; partn < vdp->v_many; pp++, partn++)
	if (pp->i_numblks <= pstartblk)
	    pstartblk -= pp->i_numblks;
	else
	    break;

/*
** pstartblk now points to the start offset within partition "partn".
** Divide the reads up between the various physical devices.
**  Make no effort to handle problems due to l2vblksz < D_PHYS_SHIFT.
*/
    do
    {
	if (partn++ > vdp->v_many)
	    panic("disk_rw: inconsistent virtual disk.");
	size = MIN(num_pblks, pp->i_numblks - pstartblk);
	physdev = &pp->i_physpart->ctlr;
	switch (cmd)
	{
	case DS_READ:
	    iofunc = Cntlrtab[physdev->type].c_read;
	    break;
	case DS_WRITE:
	    iofunc = Cntlrtab[physdev->type].c_write;
	    break;
	default:
	    panic("disk_rw: invalid disk command\n");
	}
	assert(iofunc != 0);
	assert(pstartblk+size <= pp->i_numblks);
	stat = (*iofunc)(physdev->address, physdev->unit,
					pp->i_firstblk + pstartblk, size, buf);
	if (stat != STD_OK)
	    break;
	buf += size << D_PHYS_SHIFT;
	pstartblk = 0;	/* subsequent I/O starts at beginning of a partition */
	pp++;
    } while ((num_pblks -= size) != 0);

/* Release the disk */
    mu_unlock(&Disk_sema);
    return stat;
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

errstat
disk_size(priv, l2vblksz, maxblocks)
private *	priv;		/* capability for virtual disk */
int		l2vblksz;	/* log2(virtual block size) */
disk_addr *	maxblocks;	/* returned: max # blocks that fit on disk */
{
    register int	conv_factor;	/* conversion factor */
    register objnum	vdisk;		/* number of virtual disk */
    register vdisktab *	vdp;		/* pointer to virtual disk struct */
    rights_bits		rights;		/* dummy variable for prv_decode() */

/* validate capability */
    if ((vdisk = prv_number(priv)) < 0 || vdisk > Num_virtual_disks ||
	    (vdp = Vdisktab[vdisk]) == 0 ||
	    prv_decode(priv, &rights, &vdp->v_cap.cap_priv.prv_random) < 0)
	return STD_CAPBAD;

    if (!(rights & RGT_READ))
	return STD_DENIED;

/* make sure log2(virtual block size) is sensible */
    if (l2vblksz < MIN_VBLK_SHIFT || l2vblksz > MAX_VBLK_SHIFT)
	return STD_ARGBAD;
	
/* convert physical blocks to virtual blocks, subtract # disk labels */
    if ((conv_factor = l2vblksz - D_PHYS_SHIFT) < 0)
	*maxblocks = vdp->v_numblks << -conv_factor;
    else
	*maxblocks = vdp->v_numblks >> conv_factor;
    return STD_OK;
}


/*
 * DISK_CONTROL
 *
 *	The following is a crock to handle weirdness with some disk devices.
 *	For example the Sparcstation needs this to eject floppies.
 *	Note that these operations only make sense on a physical device.
 *	Therefore we assume that the first "part" of a vdisk is the device
 *	in question.
 */

errstat
disk_control(hdr, buf, bufsz)
header *	hdr;
bufptr		buf;
bufsize		bufsz;
{
    rights_bits		rights;
    register objnum	vdisk;		/* number of virtual disk */
    register vdisktab *	vdp;		/* pointer to virtual disk struct */
    contlr *		physdev;
    errstat		(*ctl) _ARGS(( long, int, int, bufptr, bufsize ));

    hdr->h_size = 0;

    /* Validate capability */
    if ((vdisk = prv_number(&hdr->h_priv)) < 0 || vdisk > Num_virtual_disks ||
	(vdp = Vdisktab[vdisk]) == 0 ||
	prv_decode(&hdr->h_priv, &rights, &vdp->v_cap.cap_priv.prv_random) < 0)
    {
	return STD_CAPBAD;
    }

    if (!(rights & RGT_CTL))
	return STD_DENIED;

    physdev = &vdp->v_parts[0].i_physpart->ctlr;
    ctl = Cntlrtab[physdev->type].c_ctl;

    if (ctl == 0)
	return STD_COMBAD;

    return (*ctl)(physdev->address, physdev->unit, hdr->h_extra, buf, bufsz);
}


/*
** DISK_GETGEOMETRY
**
** Because the geometry information in 386/AT bus machines is not on the
** disk but in eeprom the fdisk program cannot get at it.  Therefore we
** provide this ugly hack to let it get it.
*/

errstat
disk_getgeometry(hdr, buf, size)
header *	hdr;
bufptr		buf;
bufsize		size;
{
    rights_bits		rights;		/* dummy variable for prv_decode() */
    register objnum	vdisk;		/* number of virtual disk */
    register vdisktab *	vdp;		/* pointer to virtual disk struct */
    char *		e;
    char *		p;
    geometry *		geom;
    errstat		(*getgeom)();
    contlr *		ctp;
    disklabel *		label;

/* validate capability */
    if ((vdisk = prv_number(&hdr->h_priv)) < 0 || vdisk > Num_virtual_disks ||
	(vdp = Vdisktab[vdisk]) == 0 ||
	prv_decode(&hdr->h_priv, &rights, &vdp->v_cap.cap_priv.prv_random) < 0)
    {
	hdr->h_size = 0;
	return STD_CAPBAD;
    }

    if (!(rights & RGT_READ))
    {
	hdr->h_size = 0;
	return STD_DENIED;
    }
    
/* get the geometry of the first partition used by the vdisk */
    label = &vdp->v_parts[0].i_physpart->label;
    geom = &label->d_geom;

/* if the geometry hasn't been filled in yet, we try to fill it in */
    ctp = &vdp->v_parts[0].i_physpart->ctlr;
    getgeom = Cntlrtab[ctp->type].c_geometry;
    if (geom->g_numcyl == 0 && getgeom != 0)
    {
	(void) (*getgeom)(ctp->address, ctp->unit, geom);
	label->d_chksum = 0;
	label->d_chksum = checksum((uint16 *) &label);
    }

/* and we tell the caller what we know - which may be nothing */
    e = buf + size;
    p = buf_put_int16(buf, e, geom->g_bpt);
    p = buf_put_int16(p, e, geom->g_bps);
    p = buf_put_int16(p, e, geom->g_numcyl);
    p = buf_put_int16(p, e, geom->g_altcyl);
    p = buf_put_int16(p, e, geom->g_numhead);
    p = buf_put_int16(p, e, geom->g_numsect);
    if (p == 0)
    {
	hdr->h_size = 0;
	return STD_SYSERR;
    }
    hdr->h_size = p - buf;
    return STD_OK;
}


/*
**  DISK_STD_INFO
**
**	Returns standard string describing the disk server object.
**	This includes the size of the disk in kilobytes.
*/

#define	VDISK_INFOSTRLEN	32

char 	Vdisk_info_str[VDISK_INFOSTRLEN];

static errstat
disk_std_info(hdr, buf)
header *	hdr;
char **		buf;
{
    char *	end;
    errstat	err;
    disk_addr	size;

/* see if the disk is valid and get its size */
    if ((err = disk_size(&hdr->h_priv, 10, &size)) != STD_OK)
	return err;

    end = Vdisk_info_str + VDISK_INFOSTRLEN;
    end = bprintf(Vdisk_info_str, end, "@ %8ld KB", size);

    *end = '\0';
    *buf = Vdisk_info_str;
    hdr->h_size = end - Vdisk_info_str;
    return STD_OK;
}


/*
**  DISK_STD_RESTRICT
**
**	The following implements the STD_RESTRICT command
**	There is only one rights bit anyway so it isn't too exciting.
*/

static errstat
disk_std_restrict(priv, rights_mask)
private *	priv;		/* private part of cap for virtual disk */
rights_bits	rights_mask;	/* rights to be retained in new cap */
{
    rights_bits	rights;
    objnum	vdisk;	/* virtual disk number */
    vdisktab *	vdp;	/* pointer to virtual disk */

/* validate capability */
    if ((vdisk = prv_number(priv)) < 0 || vdisk > Num_virtual_disks ||
	    (vdp = Vdisktab[vdisk]) == 0 ||
	    prv_decode(priv, &rights, &vdp->v_cap.cap_priv.prv_random) < 0)
	return STD_CAPBAD;

    rights &= rights_mask;

    if (prv_encode(priv, vdisk, rights, &vdp->v_cap.cap_priv.prv_random) < 0)
	return STD_CAPBAD;
    return STD_OK;
}


/*
** DISK_WAIT
**
**	Used by bullet server to stop it initialising until disk server
**	has been initialised.
*/

void
disk_wait()
{
    if (!Initialised)
	if (await((event) &Initialised, (interval) 0) < 0)
	    panic("disk_wait: got a signal");
}


/*
**  MK_FAKEDISK
**
**	This routine makes a fake Amoeba partition which becomes a single
**	virtual disk.   The partition does not have an Amoeba label on it
**	since that would use block 0 of the partition and we need block 0
**	for our application.  In fact this partition is the boot partition
**	and starts at physical block zero of the disk.  It is not a good
**	idea to let normal mortals near this since it probably contains the
**	partition information and the secondary bootstrap.
*/

void
mk_fake_partn(label, amlabel, disk_num, cp, unit)
disklabel *	label;		/* fake label to fill in */
disklabel *	amlabel;	/* amoeba version of host label */
objnum		disk_num;	/* the number of this disk */
confrec *	cp;		/* pointer to disk configuration table */
int		unit;		/* unit number */
{
    int		i;
    part_tab *	pp;
    disk_addr   blocksize;

/* first fill in the partition table */
    /* hand craft the first sub-partition */
    pp = label->d_ptab;
    pp->p_firstblk = -(host_partn_offset(amlabel) + 1);
    if ((Cntlrtab[cp->c_ctlr_type].c_capacity)
		(cp->c_dev_addr, unit, &pp->p_numblks, &blocksize) != STD_OK)
	pp->p_numblks = 0;
    else /* since the partition starts at -1 we add one to the size */
    {
	/* Blocksize must be power of 2 */
	assert((blocksize & (blocksize - 1)) == 0);

	if (blocksize >= D_PHYSBLKSZ)
	    pp->p_numblks = pp->p_numblks * (blocksize / D_PHYSBLKSZ) + 1;
	else
	    pp->p_numblks = pp->p_numblks / (D_PHYSBLKSZ / blocksize) + 1;
    }
    pp->p_piece = 0;
    pp->p_many = 1;
    (void) memset((_VOIDSTAR) &pp->p_cap.cap_port, 0, sizeof(port));
    uniqport(&pp->p_cap.cap_priv.prv_random);
    if (prv_encode(&pp->p_cap.cap_priv, disk_num, PRV_ALL_RIGHTS,
					&pp->p_cap.cap_priv.prv_random) < 0)
	panic("mk_fake_partn: prv_encode failed\n");

    /* the rest of the sub-partitions are null */
    for (i = 0; i < MAX_PARTNS_VDISK; i++)
    {
	pp++;
	pp->p_firstblk = 0;
	pp->p_numblks = 0;
    }

/* get disk type name, disk geometry */
    (void) strcpy(label->d_disktype, amlabel->d_disktype);
    label->d_geom = amlabel->d_geom;

/* finally put in the magic number and checksum */
    label->d_magic = AM_DISK_MAGIC;
    label->d_chksum = 0;
    label->d_chksum = checksum((uint16 *) &label);
}


/*
** DISK_THREAD
**
**	The disk thread provides the interface to threads running in user space.
**	They communicate with the disk server via transactions and all data
**	is transferred via this interface.  No direct memory mapping is
**	supported.  (Kernel threads call the disk_rw() routine directly.)
**
**  NB:  Because of Amoeba 4's small headers we do some special manipulations
**	 to protect against sign extension.  These little gremlims should come
**	 out for AMOEBA5.
*/

static void
disk_thread _ARGS((void))
{
    header	hdr;
    bufptr	buf;		/* for read/write of data */
    bufptr	replybuf;	/* pointer to reply buffer */
    bufsize	n;		/* return value of getreq */
    int		l2vblksz;	/* virtual block size in bytes */
    disk_addr	numblks;	/* # blocks to read/write */

/* make a table of virtual disks on the system */
    mu_lock(&Disk_sema);	/* need sema since disk_init can block */
    if (!Initialised)		/* do initialisation only once */
    {
	disk_init();
	Initialised++;
	wakeup((event) &Initialised);	/* see disk_wait() */
    }
    mu_unlock(&Disk_sema);

/* get a buffer to do the transactions with */
    if ((buf = (bufptr) aalloc((vir_bytes) D_REQBUFSZ, 0)) == 0)
	panic("disk_thread: Cannot allocate transaction buffer.");

    if (NULLPORT(&Diskport))
    {
	panic("Internal inconsistency - null diskport after disk_init");
    }

/* listen for and execute requests */
    for(;;)
    {
	hdr.h_port = Diskport;	/* initialised at boot time by disk_init() */
	replybuf = NILBUF;	/* always true except for READ & STD_INFO */
#ifdef USE_AM6_RPC
	n = rpc_getreq(&hdr, buf, D_REQBUFSZ);
#else
	n = getreq(&hdr, buf, D_REQBUFSZ);
#endif
	if (ERR_STATUS(n))
	{
	    printf("disk_thread: getreq failed: %d.\n", ERR_CONVERT(n));
	    panic("");
	}

	l2vblksz = (short) hdr.h_size; /*AMOEBA4*/
    /*
    ** for Amoeba 4 we are stuck with small headers so the number of blocks
    ** per read is restricted.
    ** for AMOEBA5:
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
	    if (n != 0)
		hdr.h_status = STD_ARGBAD;
	    else
		hdr.h_status = disk_std_info(&hdr, &replybuf);
	    if (ERR_STATUS(hdr.h_status))
		hdr.h_size = 0;
	    break;

	case STD_RESTRICT:
	    hdr.h_status = disk_std_restrict(&hdr.h_priv,
						    (rights_bits) hdr.h_offset);
	    hdr.h_size = 0;
	    break;

	case DS_CONTROL:
	    hdr.h_status = disk_control(&hdr, buf, D_REQBUFSZ);
	    replybuf = buf;
	    break;
	
	case DS_GETGEOMETRY:
	    hdr.h_status = disk_getgeometry(&hdr, buf, D_REQBUFSZ);
	    replybuf = buf;
	    break;

	case DS_INFO:
	    if (n != 0)
		hdr.h_status = STD_ARGBAD;
	    else
		hdr.h_status = disk_info(&hdr, &replybuf);
	    if (ERR_STATUS(hdr.h_status))
		hdr.h_size = 0;
	    break;

	case DS_READ:
	    hdr.h_status = disk_rw(DS_READ, &hdr.h_priv, l2vblksz,
						    hdr.h_offset, numblks, buf);
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
	    hdr.h_status = disk_rw(DS_WRITE, &hdr.h_priv, l2vblksz,
						    hdr.h_offset, numblks, buf);
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
#ifdef USE_AM6_RPC
	rpc_putrep(&hdr, replybuf, hdr.h_size);
#else
	putrep(&hdr, replybuf, hdr.h_size);
#endif
    }
}

#ifndef NR_VDISK_THREADS
#define NR_VDISK_THREADS	3
#endif
#ifndef VDISK_STKSIZ
#define VDISK_STKSIZ		0	/* default size */
#endif

void
initvirdisk()
{
    int	i;

    for (i = 0; i < NR_VDISK_THREADS; i++)
	NewKernelThread(disk_thread, (vir_bytes) VDISK_STKSIZ);
}
