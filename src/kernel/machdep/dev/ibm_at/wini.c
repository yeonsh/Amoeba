/*	@(#)wini.c	1.16	96/02/27 13:52:31 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * wini.c
 *
 * This file contains an implementation of a winchester hard disk
 * driver for machines with an ISA bus architecture (AT 286/386).
 * The controller is capable of driving two physical disks, although
 * the operations to either one can not be mixed. During an operation
 * (i.e. issue a command, and wait for an interrupt) it is impossible
 * to start a new command. For this reason the win_rdwr routine is
 * guarded by mutexes.
 * This driver does bad block detection and remapping into alternate
 * blocks.  Read and write requests are examined prior to execution,
 * so that blocks that give inconsistent errors will be caught as well.
 * During the disklabel phase it is possible to scan the disk
 * for bad blocks and remap them into good blocks. Bad blocks are stored
 * in a table that is located in the first track of the bad block partition.
 * The replacement blocks are allocated immediately after this table.
 * Having a special partition for bad blocks solves a lot of problems
 * that are introduced by the bootp:00 notion !
 *
 * Author:
 *	Leendert van Doorn
 * Modified:
 *	Kees Verstoep, 8 Oct 93: changed bad block remapping strategy.
 *	Kees J. Bot, 11 Jan 96: support ATA (IDE) disks.
 */
#include <assert.h>
INIT_ASSERT
#include <amoeba.h>
#include <machdep.h>
#include <cmdreg.h>
#include <stderr.h>
#include <disk/disk.h>
#include <pclabel.h>
#include <vdisk/disklabel.h>
#include <disk/conf.h>
#include <bootinfo.h>
#include <bool.h>
#include <module/mutex.h>
#include <sys/proto.h>
#include "i386_proto.h"

#include "cmos.h"
#include "wini.h"
#include "badblk.h"

#ifndef WIN_DEBUG
#define	WIN_DEBUG	0
#endif

#define WIN_MSEC_RETRY	 32000		/* controller timeout in ms */
#define WIN_TRUST_MULTI	 25		/* trust multi sector I/O after this
					 * many succeeded commands
					 */

struct wini wini[WIN_NDISKS];		/* winchester disk info */
struct badblk badblk[WIN_NDISKS];	/* bad block table */

#ifndef NDEBUG
static int win_debug;			/* current debug level */
#define dbprintf(level, list)	if (win_debug >= level) { printf list; } else 0
#else
#define dbprintf(level, list)	{}
#endif

static int win_ndisks;			/* # of disks attached */
static mutex win_rdwr_lock;		/* only one outstanding operation */
static int win_max_sectors;		/* # of sectors for multi I/O */

errstat win_read();
errstat win_write();

static void win_intr();
static void win_error();
static errstat win_do_rdwr();
static errstat win_rdwr();
static errstat win_status();
static errstat win_reset();
static errstat win_specify();
static errstat win_ready();
static errstat win_busy();
static errstat win_waitfor();
static disk_addr win_remap();
static int win_range_badblk();

/*
 * Logical to physical block translation macros:
 */
#define blk_cyl_no(unit, blk)	\
    ((blk) / (wini[unit].w_pnheads * wini[unit].w_pnsectrk))
#define blk_head_no(unit, blk)	\
    ((blk) % (wini[unit].w_pnheads * wini[unit].w_pnsectrk)) /	\
    wini[unit].w_pnsectrk
#define blk_sect_no(unit, blk)	\
    (((blk) % wini[unit].w_pnsectrk) + 1)

/*
 * The controller is initialized with the disk parameters fetched
 * from BIOS by the bootstrap code. These parameters describe the
 * geometry of the (possibly) two winchester drives, the remaining
 * geometry information is computed from it.
 */
/* ARGSUSED */
errstat
win_devinit(devaddr, irq)
    long devaddr;		/* don't care */
    int irq;			/* interrupt request level */
{
    struct bootinfo *bootinfo;
    int i, j, type, fdt, tryata;
    unsigned char id_data[WIN_SECTOR_SIZE];
    char id_string[41];
    unsigned int status;

#define id_byte(n)	(&id_data[2 * (n)])
#define id_word(n)	(((uint16) id_byte(n)[0] <<  0) \
			|((uint16) id_byte(n)[1] <<  8))
#define id_longword(n)	(((uint32) id_byte(n)[0] <<  0) \
			|((uint32) id_byte(n)[1] <<  8) \
			|((uint32) id_byte(n)[2] << 16) \
			|((uint32) id_byte(n)[3] << 24))

    mu_init(&win_rdwr_lock);
    mu_lock(&win_rdwr_lock);

#ifndef NDEBUG
    if ((win_debug = kernel_option("win")) == 0)
	win_debug = WIN_DEBUG;
#endif

    win_max_sectors = WIN_MAX_SECTORS;

    /*
     * Determine the number of disks attached (from CMOS RAM), and
     * retrieve its geometry information from the boot information.
     */
    fdt = cmos_read(CMOS_FDTB);
    bootinfo = (struct bootinfo *)BI_ADDR;
    for (i = 0, type = fdt >> 4; i < WIN_NDISKS; i++, type = fdt) {
	if ((type & 0x0F) == CMOS_NOFDSK)
	    continue;
	if (bootinfo->bi_magic != BI_MAGIC) {
	    /* do not complain when there are no disks attached */
	    printf("win_devinit: invalid boot info; winchester driver disabled\n");
	    return STD_SYSERR;
	}
	wini[i].w_lncyl = bootinfo->bi_hdinfo[i].hdi_ncyl - 1;
	wini[i].w_lnheads = bootinfo->bi_hdinfo[i].hdi_nheads;
	wini[i].w_precomp = bootinfo->bi_hdinfo[i].hdi_precomp;
	wini[i].w_ctrl = bootinfo->bi_hdinfo[i].hdi_ctrl;
	wini[i].w_lnsectrk = bootinfo->bi_hdinfo[i].hdi_nsectrk;
	dbprintf(1, ("ncyl %d, nheads %d, precomp %x, ctrl %x, sec/trk %d\n",
		     bootinfo->bi_hdinfo[i].hdi_ncyl,
		     bootinfo->bi_hdinfo[i].hdi_nheads & 0xFF,
		     bootinfo->bi_hdinfo[i].hdi_precomp,
		     bootinfo->bi_hdinfo[i].hdi_ctrl & 0xFF,
		     bootinfo->bi_hdinfo[i].hdi_nsectrk&0xFF));
	win_ndisks++;
    }
    if (win_ndisks == 0) return STD_SYSERR;

    /* setup interrupt vector and initialize the controller */
    setirq(WIN_IRQ, win_intr);
    pic_enable(WIN_IRQ);

    /* Are these disks ATA devices, or normal AT disks? */
    for (i = 0; i < win_ndisks; i++) {
	/* Only try a disk for ATA capabilities if the logical number of
	 * cylinders <= 1024.  A disk with more than 1024 cylinders is weird,
	 * so we don't want to know if it knows ATA commands or not, we
	 * assume *not*.
	 */
	tryata = (wini[i].w_lncyl <= 1024);

	if (tryata) {
	    out_byte(WIN_HF,  (int) wini[i].w_ctrl);
	    out_byte(WIN_SDH, i << 4 | 0xA0);
	    out_byte(WIN_CMD, ATA_IDENTIFY);

	    /* wait for the controller to accept the parameters */
	    (void) await((event) wini, (interval) WIN_TIMEOUT);

	    /* check controller status */
	    if (win_waitfor(WIN_BUSY, 0) == STD_OK) {
		status = in_byte(WIN_STATUS);
	    } else {
		status = WIN_BUSY;
	    }
	}

	if (tryata && (status & (WIN_IMASK|WIN_BUSY)) == (WIN_READY|WIN_DONE)) {
	    /* This is an ATA device, get device information. */
	    ins_word(WIN_DATA, (char *) id_data, WIN_SECTOR_SIZE);

	    /* Why are the strings byte swapped??? */
	    for (j = 0; j < 40; j++) id_string[j] = id_byte(27)[j^1];
	    while (j > 0 && id_string[--j] == ' ') id_string[j] = 0;
	    id_string[40] = '\0';

	    /* Preferred CHS translation mode. */
	    wini[i].w_pncyl = id_word(1);
	    wini[i].w_pnheads = id_word(3);
	    wini[i].w_pnsectrk = id_word(6);
	    wini[i].w_size = wini[i].w_pncyl *
			wini[i].w_pnheads * wini[i].w_pnsectrk;
	    printf("Winchester %d: %s (%lu MB)\n",
		i, id_string, wini[i].w_size / (2 * 1000));

	    dbprintf(1, ("Winchester %d: identify data:\n", i));
	    dbprintf(1, ("config = %04x (RMB=%d, !RMD=%d)\n",
		    id_word(0), (id_word(0) >> 7)&1,
		    (id_word(0) >> 6)&1));
	    dbprintf(1, ("default cylinders = %u\n", id_word(1)));
	    dbprintf(1, ("default heads = %u\n", id_word(3)));
	    dbprintf(1, ("default sectors = %u\n", id_word(6)));
	    if (id_word(10) != 0) {
		    dbprintf(1, ("serial = "));
		    for (j= 0; j < 20; j++)
			dbprintf(1, ("%c", id_byte(10)[j^1]));
		    dbprintf(1, ("\n"));
	    }
	    if (id_word(23) != 0) {
		    dbprintf(1, ("revision = "));
		    for (j= 0; j < 8; j++)
			dbprintf(1, ("%c", id_byte(23)[j^1]));
		    dbprintf(1, ("\n"));
	    }
	    if (id_word(27) != 0) {
		    dbprintf(1, ("model = "));
		    for (j= 0; j < 40; j++)
			dbprintf(1, ("%c", id_byte(27)[j^1]));
		    dbprintf(1, ("\n"));
	    }
	    dbprintf(1, ("max multiple = %u\n", id_byte(47)[0]));
	    dbprintf(1, ("capabilities = %04x (stdSTANDBY=%d, suppIORDY=%d, ",
		    id_word(49), (id_word(49) >> 13)&1,
		    (id_word(49) >> 11)&1));
	    dbprintf(1, ("disIORDY=%d, LBA=%d, DMA=%d)\n",
		    (id_word(49) >> 10)&1, (id_word(49) >> 9)&1,
		    (id_word(49) >> 8)&1));
	    dbprintf(1, ("LBA sectors = %lu\n", id_longword(60)));
	} else {
	    /* Not an ATA device; no translations, no special features. */
	    if (tryata) {
		dbprintf(1, ("not ATA: status=%02x, error=%02x\n",
				in_byte(WIN_STATUS), in_byte(WIN_ERROR)));
	    }
	    wini[i].w_pncyl = wini[i].w_lncyl;
	    wini[i].w_pnheads = wini[i].w_lnheads;
	    wini[i].w_pnsectrk = wini[i].w_lnsectrk;
	    wini[i].w_size = (wini[i].w_pncyl - PCL_PARKCYLS) *
			wini[i].w_pnheads * wini[i].w_pnsectrk;
	    printf(
	    	"Winchester %d: %u cylinders, %u heads, %u sectors (%lu MB)\n",
		i, wini[i].w_pncyl, wini[i].w_pnheads, wini[i].w_pnsectrk,
		wini[i].w_size / (2 * 1000));
	}
    }

    if (win_specify() != STD_OK) {
	mu_unlock(&win_rdwr_lock);
	printf("win_devinit: initialization failed\n");
	return STD_SYSERR;
    }

    mu_unlock(&win_rdwr_lock);
    dbprintf(2, ("win_devinit: initialization was successful\n"));
    return STD_OK;
}

/*
 * Initialize a unit (i.e. a physical disk).
 * Since they are already initialized, this
 * mainly consists of reading the bad block table.
 */
errstat
win_unitinit(devaddr, unit)
    long devaddr;		/* don't care */
    int unit;			/* unit to use */
{
    register struct pcpartition *pe;
    register int i, last;
    char pclabel[PCL_BOOTSIZE];

    /* some standard sanity checks */
    if (unit < 0 || unit >= win_ndisks)
	return STD_SYSERR;

    /* zero bad blocks in advance */
    badblk[unit].bb_count = 0;

    /* read partition table */
    (void) win_read(devaddr, unit, (disk_addr) 0, (disk_addr) 1, pclabel);
    if (!PCL_MAGIC(pclabel)) return STD_OK;

    /* search for bad block partition */
    for (i = 0, pe = PCL_PCP(pclabel); i < PCL_NPART; i++, pe++)
	if (pe->pp_sysind == PCP_BADBLK)
	    break;
    if (i == PCL_NPART) return STD_OK;

    /* read bad block table */
    (void) win_read(devaddr, unit, (disk_addr) pe->pp_first,
		    (disk_addr) BB_BBT_SIZE, (char *) &badblk[unit]);
    if (badblk[unit].bb_magic != BB_MAGIC ||
	badblk[unit].bb_count > BB_NBADMAP)
    {
	printf("WARNING: malformed bad block table: wini controller at %lx, unit %d.\n",
	    devaddr, unit);
	/* invalidate bad block count */
	badblk[unit].bb_count = 0;
    }

    dbprintf(2, ("win_unitinit: unit %d: size = %ld; # bad blocks = %ld\n",
		 unit,  wini[unit].w_size, badblk[unit].bb_count));
    /*
     * Sort the bad block table for increased lookup speed.
     * Simple bubble sort is good enough here.
     */
    last = badblk[unit].bb_count;
    for (i = 0; i < last; i++) {
	struct badmap *bp_i;
	register struct badmap *bp, *bp_lowest, *ep;
	
	ep = &badblk[unit].bb_badmap[last];
	bp_i = &badblk[unit].bb_badmap[i];
	bp_lowest = bp_i;

	for (bp = bp_i + 1; bp < ep; bp++) {
	    if (bp->bb_bad < bp_lowest->bb_bad) {
		bp_lowest = bp;
	    }
	}
	if (bp_lowest != bp_i) { /* swap entries */
	    struct badmap tmp_map;
	    
	    tmp_map = *bp_i;
	    *bp_i = *bp_lowest;
	    *bp_lowest = tmp_map;
	}
    }

    for (i = 0; i < badblk[unit].bb_count; i++) {
	disk_addr bad, alt;

	bad = badblk[unit].bb_badmap[i].bb_bad;
	alt = badblk[unit].bb_badmap[i].bb_alt;
	dbprintf(3, ("bad #%d: %ld (cyl %d, hd %d, sec %d) -> ",
		     i, bad, blk_cyl_no(unit, bad), blk_head_no(unit, bad),
		     blk_sect_no(unit, bad)));
	dbprintf(3, ("%ld (%d, %d, %d)\n", alt, blk_cyl_no(unit, alt),
		     blk_head_no(unit, alt), blk_sect_no(unit, alt)));
    }
    return STD_OK;
}

/*
 * Since all winchester disks are fixed,
 * it isn't very hard to figure out its
 * unit type.
 */
/* ARGSUSED */
int
win_unittype(devaddr, unit)
    long devaddr;               /* don't care */
    int unit;                   /* unit to use */
{
    return UNITTYPE_FIXED;
}

/*
 * Return disk geometry information for specified unit
 */
/* ARGSUSED */
errstat
win_geometry(devaddr, unit, geom)
    long devaddr;               /* don't care */
    int unit;                   /* unit to use */
    geometry *geom;		/* disk geometry info */
{
    /* some standard sanity checks */
    if (unit < 0 || unit >= win_ndisks)
	return STD_SYSERR;
    geom->g_bps = WIN_SECTOR_SIZE;
    geom->g_bpt = wini[unit].w_lnsectrk * WIN_SECTOR_SIZE;
    geom->g_numcyl = wini[unit].w_lncyl;
    geom->g_altcyl = 0;
    geom->g_numhead = wini[unit].w_lnheads;
    geom->g_numsect = wini[unit].w_lnsectrk;
    return STD_OK;
}


/*
 * Read/Write blocks from/to disk, remapping bad blocks when needed.
 */
static errstat
win_do_rdwr(opcode, unit, strt_blk, blk_cnt, buf_ptr)
    int opcode;
    int unit;			/* unit to use */
    disk_addr strt_blk;		/* first block */
    disk_addr blk_cnt;		/* total number of blocks */
    char *buf_ptr;		/* source/destination */
{
    errstat err;		/* error status of the win_rdwr command */
    disk_addr chunk;		/* number of blocks read in one command */

    /* some standard sanity checks */
    if (unit < 0 || unit >= win_ndisks || strt_blk < 0 || blk_cnt < 0)
	return STD_ARGBAD;

    /* check the disk bounds */
    if (strt_blk + blk_cnt > wini[unit].w_size)
	return STD_ARGBAD;

    /* split big reads into managable chunks */
    do {
	chunk = MIN(blk_cnt, win_max_sectors);
	if (win_range_badblk(unit, strt_blk, strt_blk + chunk - 1) >= 0) {
	    /* Oops: this block range contains a bad block.
	     * We just do the blocks in this chunk one by one.
	     * This is simple, but not really optimal.
	     */
	    do {
		disk_addr remap = win_remap(unit, strt_blk);

		err = win_rdwr(unit, remap, (disk_addr) 1, buf_ptr, opcode);
		buf_ptr += WIN_SECTOR_SIZE;
		strt_blk++;
		blk_cnt--;
		chunk--;
	    } while (chunk > 0 && err == STD_OK);
	} else {
	    err = win_rdwr(unit, strt_blk, chunk, buf_ptr, opcode);
	    buf_ptr += chunk * WIN_SECTOR_SIZE;
	    strt_blk += chunk;
	    blk_cnt -= chunk;
	}
    } while (err == STD_OK && blk_cnt > 0);

    return err;
}

/*
 * Read blocks from disk
 */
/* ARGSUSED */
errstat
win_read(devaddr, unit, strt_blk, blk_cnt, buf_ptr)
    long devaddr;		/* don't care */
    int unit;			/* unit to use */
    disk_addr strt_blk;		/* first block to read */
    disk_addr blk_cnt;		/* total number of blocks to be read */
    char *buf_ptr;		/* destination for the read data */
{
    return win_do_rdwr(WIN_READ, unit, strt_blk, blk_cnt, buf_ptr);
}

/*
 * Write blocks to disk
 */
/* ARGSUSED */
errstat
win_write(devaddr, unit, strt_blk, blk_cnt, buf_ptr)
    long devaddr;		/* don't care */
    int unit;			/* unit to use */
    disk_addr strt_blk;		/* first block to write */
    disk_addr blk_cnt;		/* total number of blocks to be written */
    char *buf_ptr;		/* source for the write */
{
    return win_do_rdwr(WIN_WRITE, unit, strt_blk, blk_cnt, buf_ptr);
}

/*
 * Gets capacity of the specified unit
 */
/* ARGSUSED */
errstat
win_capacity(devaddr, unit, lastblk, blksz)
    long devaddr;		/* don't care */
    int unit;			/* unit to use */
    int32 *lastblk;		/* out: pointer to # blocks on device */
    int32 *blksz;		/* out: pointer to size of logical block */
{
    if (unit < 0 || unit >= win_ndisks)
	return STD_ARGBAD;
    *blksz = (int32) WIN_SECTOR_SIZE;
    *lastblk = (int32) wini[unit].w_size;
    return STD_OK;
}

/*
 * Park the disk heads at the last cylinder
 */
void
win_stop()
{
    register int unit;

    pic_disable(WIN_IRQ);
    for (unit = 0; unit < win_ndisks; unit++) {
	/*
	 * Seek the read/write head to the last cylinder
	 * on the disk. This prevents disk damages which
	 * are caused by bouncing the head on the disk.
	 */
	out_byte(WIN_HF, (int) wini[unit].w_ctrl);
	out_byte(WIN_CYLLSB, (int) wini[unit].w_pncyl);
	out_byte(WIN_CYLMSB, (int) (wini[unit].w_pncyl >> 8));
	out_byte(WIN_SDH, 0xA0 | (unit << 4));
	out_byte(WIN_CMD, WIN_SEEK);

	/*
	 * Wait for controller to finish the command. Cannot wait for the
	 * interrupt, since win_stop might be called from low interrupt
	 * level (see keyboard.c).
	 */
	(void) win_ready();
        if (win_status() != STD_OK)
	    printf("win_stop: unit %d: head park failed\n", unit);
    }
}

/*
 * Interrupt handler
 */
/* ARGSUSED */
static void
win_intr(irq)
    int irq;
{
    enqueue((void (*) _ARGS((long))) wakeup, /* (event) */ (long) wini);
}

/*
 * Low level I/O routine that actually does the operation and
 * returns an appropriate status. This function computes the
 * physical location of the requested data on disk and instructs
 * the controller accordingly.
 * This routine is guarded by mutexes to ensure that only one
 * request is active. Although the controller can drive two disks,
 * there can only be one outstanding operation.
 * If a bad block is detected during an I/O operation it is
 * replaced by its alternate block found in the bad block table,
 * and the operation is restarted. If such a block could not be
 * found we regard it as a fatal error.
 */
static errstat
win_rdwr(unit, start, count, data, opcode)
    int unit;			/* unit to use */
    disk_addr start;		/* starting block */
    disk_addr count;		/* total number of blocks to be transfer */
    char *data;			/* place to read from/write to */
    int opcode;
{
    disk_addr cylinder;		/* physical cylinder on disk */
    int head;			/* physical head on disk */
    int status;			/* value from status register */
    int error;			/* value from error register */
    int retry = 0;		/* retry counter */
    errstat err;		/* error status */
    char *ptr = data;		/* winchester data */
    disk_addr strtblk = start;	/* start block */
    disk_addr blk_cnt = count;	/* number of blocks */
    static int did_multi_sector = 0; /* try to keep doing multi sector I/O */

    /* only one outstanding operation allowed */
    mu_lock(&win_rdwr_lock);

restart:

    /* compute the physical sector/cylinder/head  */
    cylinder = blk_cyl_no(unit, strtblk);
    head = blk_head_no(unit, strtblk);

    if (retry > 0) {
	dbprintf(1,("win_rdwr: %s unit %d, blk %ld, cyl %d, head %d, sec %d\n",
		    (opcode == WIN_READ) ? "reading" : "writing",
		    unit, strtblk, cylinder, head,
		    blk_sect_no(unit, strtblk)));
    }

    /* wait for controller to become ready */
    if (win_ready() != STD_OK) {
	printf("win_rdwr: controller not ready\n");

	/* give it another try before giving up */
	if (win_reset() == STD_OK && win_ready() == STD_OK) {
	    printf("win_rdwr: controller OK\n");
	} else {
            mu_unlock(&win_rdwr_lock);
	    printf("win_rdwr: controller still not ready\n");
	    return STD_SYSERR;
	}
    }

    /*
     * Output a command block to the controller. The command block
     * consists of 8 bytes representing: the control byte, write pre-
     * compensation cylinder, number of blocks (in physical sectors),
     * the physical sector number (relative to track!), the LSB of
     * the cylinder, the MSB of the cylinder, an auxilary byte specifying
     * the disk (two possible) and the head to be used, and finaly
     * the read or write opcode.
     */
    out_byte(WIN_HF,      (int) wini[unit].w_ctrl);
    out_byte(WIN_PRECOMP, (int) wini[unit].w_precomp >> 2);
    out_byte(WIN_COUNT,   (int) blk_cnt);
    out_byte(WIN_SECTOR,  (int) blk_sect_no(unit, strtblk));
    out_byte(WIN_CYLLSB,  (int) cylinder);
    out_byte(WIN_CYLMSB,  (int) cylinder >> 8);
    out_byte(WIN_SDH,     (int) ((unit << 4) | (uint8)head | 0xA0));
    out_byte(WIN_CMD,     opcode);

    /*
     * When writing a sector, we first put the data to the controller
     * and wait for the acknowledging interrupt. Upon reading the
     * requested data is read after the interrupt occurs.
     */
    if (opcode == WIN_WRITE) {
	do {
	    if (win_waitfor(WIN_DRQ, WIN_DRQ) != STD_OK) {
		mu_unlock(&win_rdwr_lock);
		printf("win_rdwr: controller does not request data\n");
		return STD_SYSERR;
	    }

	    outs_word(WIN_DATA, ptr, WIN_SECTOR_SIZE);

	    /* wait for acknowledgement */
	    (void) await((event) wini, (interval) WIN_TIMEOUT);

	    /* check controller status */
	    if (win_waitfor(WIN_BUSY, 0) == STD_OK) {
		status = in_byte(WIN_STATUS);
	    } else {
		status = WIN_BUSY;
	    }

	    if ((status & (WIN_IMASK|WIN_BUSY)) != (WIN_READY|WIN_DONE)) break;

	    strtblk++;
	    ptr += WIN_SECTOR_SIZE;
	} while (--blk_cnt > 0);
    } else {
	/* WIN_READ request */
	do {
	    /* wait for acknowledgement */
	    (void) await((event) wini, (interval) WIN_TIMEOUT);

	    /* check controller status */
	    if (win_waitfor(WIN_BUSY, 0) == STD_OK) {
		status = in_byte(WIN_STATUS);
	    } else {
		status = WIN_BUSY;
	    }

	    if ((status & (WIN_IMASK|WIN_BUSY)) != (WIN_READY|WIN_DONE)) break;

	    ins_word(WIN_DATA, ptr, WIN_SECTOR_SIZE);

	    strtblk++;
	    ptr += WIN_SECTOR_SIZE;
	} while (--blk_cnt > 0);
    }

    /*
     * The following situation is quite serious and seems to happen
     * on faulty hardware or hardware that doesn't support multi
     * sector I/O. Disseminating between the two is hard, so we
     * first assume the controller can't do multi sector I/O (in
     * which case we retry the whole command block by block), and
     * in case we get here again we assume that the hardware is
     * faulty and abort. As a precaution we hit the controller on
     * the head first.
     */
    if (status & WIN_BUSY) {
	if (win_max_sectors > 1 && did_multi_sector < WIN_TRUST_MULTI) {
	    win_max_sectors = 1;
	    printf("WARNING: winchester multi sector I/O functionality turned off\n");
	} else {
	    printf("win_rdwr: still busy?\n");
	}

	if (win_reset() != STD_OK) {
	    mu_unlock(&win_rdwr_lock);
	    printf("win_rdwr: controller won't reset\n");
	    return STD_SYSERR;
	}

	/*
	 * Although a bit hackish, this will restart the command
	 */
	ptr = data; strtblk = start; blk_cnt = count;
    }

    /*
     * When an operation is acknowledged by means of an interrupt,
     * its success or failure is denoted by the contents of the
     * status register. If the controller didn't mark the command
     * as done and did not become ready we are in serious trouble.
     *
     * The errors we can recover from are a bad block mark (these
     * blocks are physically marked as bad), ID not found, address
     * mark not found, and unrecoverable ECC error. These later
     * three are only remapped when the retry count has been
     * exceeded.
     *
     * In case of another serious error we try to hit the
     * controller as hard as we can, and restart the operation.
     * If this doesn't work after a limit number of retries we
     * give up.
     */
    if (blk_cnt > 0) {
	/* If more than one block remains from a multisector request
	 * then the request is restarted block by block.
	 */
	if (blk_cnt > 1) {
	    mu_unlock(&win_rdwr_lock);

	    do {
		err = win_rdwr(unit, strtblk, (disk_addr) 1, ptr, opcode);
		ptr += WIN_SECTOR_SIZE;
		strtblk++;
	    } while (err == STD_OK && --blk_cnt > 0);

	    return err;
	}

	if (status & WIN_ERR) {
	    error = in_byte(WIN_ERROR);
	    win_error(error);
	    if ((error & WIN_BADBLK) ||	(retry > WIN_ERROR_RETRY &&
				   (error & (WIN_IDNF|WIN_ADDRMARK|WIN_ECC))))
	    {
		dbprintf(1, ("win_rdwr(%s): bad block #%d detected\n",
			     (opcode == WIN_WRITE) ? "writing" : "reading",
			     strtblk));
	    }
	}

	if (++retry > WIN_ERROR_RETRY) {
	    /* Serious error: exceeded retry limit.  Don't try to fix
	     * this here: block remapping is done at a higher level.
	     */
	    dbprintf(1, ("win_rdwr: stop %s block %ld after %d tries\n",
			 (opcode == WIN_WRITE) ? "writing" : "reading",
			 strtblk, retry));
			 
	    mu_unlock(&win_rdwr_lock);
	    return STD_SYSERR;
	}

	if ((status & WIN_BUSY) == 0) {
	    /* if WIN_BUSY is set, the reset was done already */
	    (void) win_reset();
	}

	/* Now restart the command */
	goto restart;
    } else {
	if (count > 1 && win_max_sectors > 1) {
	    /* Succesfully performed a multi sector I/O command.
	     * Increase the counter so that we don't turn it off
	     * unnecessarily later on.
	     */
	    did_multi_sector++;
	}
    }

    mu_unlock(&win_rdwr_lock);
    return STD_OK;
}

/*
 * Bad block remapping support.
 */

static int
win_range_badblk(unit, start_range, end_range)
    int unit;			/* unit to use */
    disk_addr start_range;
    disk_addr end_range;
{
    /*
     * See if this unit has a bad block in range [start_range..end_range].
     * If so, return the index of the first bad block in the bad block table.
     * Otherwise return -1.
     */
    int first, last;
    register int i;

    if (badblk[unit].bb_magic != BB_MAGIC) {
	/* no bad block table for this unit */
	return -1;
    }

    /*
     * Since the bad blocks are sorted, we can use binary search to
     * reduce the search range.
     */
    first = 0;
    last = badblk[unit].bb_count - 1;
    while (first < last) {
	int middle = (first + last) / 2;
	disk_addr middle_block = badblk[unit].bb_badmap[middle].bb_bad;

	if (middle_block > end_range) {
	    last = middle - 1;
	} else if (middle_block < start_range) {
	    first = middle + 1;
	} else {
	    break;
	}
    }

    /* now see if one of first..last is contained in the range */
    for (i = first; i <= last; i++) {
	disk_addr bad = badblk[unit].bb_badmap[i].bb_bad;

	if (bad >= start_range && bad <= end_range) {
	    dbprintf(1, ("win_range_badblk: [%ld..%ld]: bad block #%d = %ld\n",
			 start_range, end_range, i, bad));
	    return i;
	}
    }

    /* no bad blocks in given range */
    return -1;
}

static disk_addr
win_remap(unit, blk)
    int unit;			/* unit to use */
    disk_addr blk;		/* block to be remapped if it is bad */
{
    /* Map disk block upon alternate block if it is in the bad block table;
     * win_range_badblk() does all the work.
     */
    int index;

    if ((index = win_range_badblk(unit, blk, blk)) >= 0) {
	disk_addr remap;

	assert(badblk[unit].bb_badmap[index].bb_bad == blk);
	remap = badblk[unit].bb_badmap[index].bb_alt;

	dbprintf(1, ("win_remap: bad block #%d: %ld (%d, %d, %d) -> ",
		     index, blk, blk_cyl_no(unit, blk),
		     blk_head_no(unit, blk), blk_sect_no(unit, blk)));
	dbprintf(1, ("%ld (%d, %d, %d)\n",
		     remap, blk_cyl_no(unit, remap),
		     blk_head_no(unit, remap), blk_sect_no(unit, remap)));
	return remap;
    }

    /* no remapping */
    return blk;
}

/*
 * Reset the controller. This is done after any catastrophe,
 * like the controller refusing to respond.
 */
static errstat
win_reset()
{
    register int i;
    int	error;

    dbprintf(2, ("win_reset: resetting controller\n"));

    /* Wait for any internal drive recovery. */
    pit_delay(100);

    /*
     * This little magic turns the strobe bit low,
     * hence resetting the winchester controller
     */

    out_byte(WIN_HF, 4);
    pit_delay(10); /* at least 10 milli seconds */
    out_byte(WIN_HF, (int) wini[0].w_ctrl & 0x0F);
    pit_delay(10); /* just to be sure */

    for (i = 0; i < WIN_RETRY && win_busy() != STD_OK; i++)
	/* nothing */;
    if (win_busy() != STD_OK) {
	printf("win_reset: reset failed - drive busy\n");
	return STD_SYSERR;
    }

    error = in_byte(WIN_ERROR);
    if (error != WIN_ADDRMARK) {
	win_error(error);
	return STD_SYSERR;
    }

    win_specify();
}

/*
 * Specify the parameters of the disks.
 */
static errstat
win_specify()
{
    register int i;
    int	error;

    for (i = 0; i < win_ndisks; i++) {
	/* wait for controller to become ready */
	if (win_ready() != STD_OK) {
	    printf("win_specify: controller not ready\n");
	    return STD_SYSERR;
	}

	/*
	 * Reinitialize the controller by sending a new parameter
	 * block to it. This block consists of the disk's control
	 * byte, precompensation cylinder, number of sectors per
	 * track and the number of heads.
	 */
	out_byte(WIN_HF,      (int) wini[i].w_ctrl);
	out_byte(WIN_PRECOMP, (int) wini[i].w_precomp >> 2);
	out_byte(WIN_COUNT,   (int) wini[i].w_pnsectrk);
	out_byte(WIN_CYLLSB,  0);
	out_byte(WIN_SDH,     (int) (i << 4 | (wini[i].w_pnheads - 1) | 0xA0));
	out_byte(WIN_CMD,     WIN_SETPARAM);

	/* wait for the controller to accept the parameters */
	(void) await((event) wini, (interval) WIN_TIMEOUT);

	if (win_status() != STD_OK) {
	    printf("win_specify: win_status failed\n");
	    return STD_SYSERR;
	}

	/* wait for controller to become ready */
	if (win_ready() != STD_OK) {
	    printf("win_specify: controller not ready\n");
	    return STD_SYSERR;
	}

	/*
	 * Having initialized the controller (thus
	 * zeroing its internal counters) we have to
	 * restore the disk's arm to track 0 to re-
	 * calibrate the controller's notion of its
	 * position.  Don't check if the operation
	 * succeeds, recalibrate is an optional command
	 * under ATA.
	 */
	out_byte(WIN_HF,  (int) wini[i].w_ctrl);
	out_byte(WIN_SDH, i << 4 | 0xA0);
	out_byte(WIN_CMD, WIN_RESTORE);

	/* wait for the controller to accept the parameters */
	(void) await((event) wini, (interval) WIN_TIMEOUT);
    }

    return STD_OK;
}

static errstat
win_waitfor(mask, value)
int mask;	/* status mask */
int value;	/* required status */
{
    /* Wait until controller is in the required state.  Return STD_SYSERR
     * on timeout.  The controller is polled fast at first, because it usually
     * responds early, then once per millisecond to catch it when it's slow.
     */
    int retries = WIN_READY_RETRY + WIN_MSEC_RETRY;

    do {
	if ((in_byte(WIN_STATUS) & mask) == value) {
	    return STD_OK;
	}
	if (retries < WIN_MSEC_RETRY) {
	    pit_delay(1);
	}
    } while (--retries != 0);

    printf("wini: need reset\n");
    return STD_SYSERR;
}

/*
 * Check whether the resulting status is sane
 */
static errstat
win_status()
{
    int status;

    status = in_byte(WIN_STATUS);
    if ((status&WIN_BUSY) == 0 && (status&WIN_IMASK) != (WIN_READY|WIN_DONE)) {
	if (status & WIN_ERR)
	    win_error(in_byte(WIN_ERROR));
	return STD_SYSERR;
    }
    return STD_OK;
}

/*
 * Wait for the controller to become ready to receive a command
 */
static errstat
win_busy()
{
    int status;

    (void) win_ready();
    status = in_byte(WIN_STATUS);
    if ((status & (WIN_BUSY|WIN_READY|WIN_DONE)) != (WIN_READY|WIN_DONE)) {
	return STD_SYSERR;
    }
    return STD_OK;
}

/*
 * Wait for the controller to become ready or timeout
 */
static errstat
win_ready()
{
    register int i;

    for (i = 0; i < WIN_READY_RETRY; i++)
	if ((in_byte(WIN_STATUS) & (WIN_BUSY|WIN_READY)) == WIN_READY)
	    return STD_OK;
    return STD_SYSERR;
}

static void
win_error(error)
    int error;
{
    unsigned sdh;

    switch (error) {
    case WIN_ADDRMARK:
	printf("win_error: missing data address mark");
	break;
    case WIN_TRACK0:
	printf("win_error: track 0 error");
	break;
    case WIN_ABORT:
	printf("win_error: command aborted");
	break;
    case WIN_IDNF:
	printf("win_error: ID not found");
	break;
    case WIN_ECC:
	printf("win_error: data ECC error");
	break;
    case WIN_BADBLK:
	printf("win_error: bad block detected");
	break;
    default:
	printf("win_error: unknown error");
	break;
    }
    
    sdh = in_byte(WIN_SDH);
    printf(": disk %d, ", (sdh >> 4) & 0x01);
    printf("cyl %d, ", (in_byte(WIN_CYLMSB) << 8) | in_byte(WIN_CYLLSB));
    printf("head %d, ", sdh & 0x0F);
    printf("sector %d, ", in_byte(WIN_SECTOR));
    printf("count %d\n", in_byte(WIN_COUNT));
}
