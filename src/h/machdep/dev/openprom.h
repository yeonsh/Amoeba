/*	@(#)openprom.h	1.6	96/02/27 10:30:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __OPENPROM_H__
#define __OPENPROM_H__

/*
 * openprom.h -- structure definitions for Sun's 'OPENPROM' interface.
 *		 Based on IEEE draft standard P1275 and years of trying
 *		 to understand earlier versions of the so-called openprom.
 *
 * At time of writing there have been three versions of the openprom
 * interface used in Sparcstations.  They are in some respects incompatible
 * with one another and it is important to know which version you have before
 * trying to use it.
 *
 * From Phil Shafer:
 *
 * To my knowledge (and that may not say much), the Sun OPENPROM wants to
 * eat the following memory management resources:
 *	First and foremost, the prom eats all virtual addresses between
 *	0xFFD00000 and 0xFFF00000, and all memory and segments used by
 *	anything in this area. (We will give it from ffd00000 to max, also)
 *
 * 	The first 0x1B pages of memory (though when I can take them back is
 *	not clear).
 *
 *	The first PMEG (#0) (for mapping the first 1B pages)
 *
 *	The memory marked unavailable at the end of the first chunk of
 *	**prom_devp->pd_availmem. This is used for variables and stack.
 *	It is different in newer releases of the openprom - there you
 *	are simply told what is still available as sets of address ranges
 *	under the attribute "memory" in the CPU node.
 *
 *	The last 8 PMEG numbers (eg. 0x78 thru 0x7F on SS1, 0xF8 thru 0xFF
 *	on the SS2), for various mappings.
 *
 *	PMEG #0x37 or perhaps #0x3c (memory fails me), which the prom
 *	uses to access the prom IO device. It steals this back, whenever
 *	the iodev gets used, but this should not bother us.
 *
 * If you know of anything else, let me know......
 *
 * Author: Gregory J. Sharp, November 1992
 */

typedef	unsigned int	ihandle_t;
typedef	unsigned int	phandle_t;
typedef	unsigned int	dnode_t;
typedef	char *		caddr_t;
typedef	void *		addr_t;
typedef unsigned int	uint_t;

#define	MAXDEVNAME	128	/* longest legal pathname for prom device */
#define	NONODE		(dnode_t) 0
#define	BADNODE		(dnode_t) -1

/*
 * For device tree traversal.
 *	The openprom builds a list of devices into a tree structure.
 *	Different versions of the prom have different "devices" included
 *	so it is hard to be truly portable across all versions of the
 *	"openprom".  (e.g., memory is in the device tree in version 2 and
 *	later but not in version 1 - so you have to count it for SS1s.)
 *	
 */

struct config_ops
{
    dnode_t	(*dp_next)   (/* dnode_t */);
    dnode_t	(*dp_child)  (/* dnode_t */);
    int		(*dp_gplen)  (/* dnode_t, caddr_t */);
    int		(*dp_getprop)(/* dnode_t, caddr_t, addr_t */);
    int		(*dp_setprop)(/* dnode_t, caddr_t, addr_t, int */);
    caddr_t	(*dp_nxtprop)(/* dnode_t, caddr_t */);
};


/*
 * This structure represents one piece of bus space occupied by a given
 * device.  It is used in an array for devices with multiple address windows.
 */
typedef struct dev_reg dev_reg_t;
struct dev_reg {
    uint_t	reg_bustype;	/* Cookie for type of bus it is on */
    uint_t	reg_addr;	/* Address of register relative to the bus */
    uint_t	reg_size;	/* Size of register set (in bytes?) */
};


/*
 * Version 1 only
 ****************
 *
 * Some structs needed for getting the data out of the OPENPROM interface.
 */

typedef struct prom_iodev prom_iodev_t;
struct prom_iodev {
    char		pi_devname[2];      	/* ie. "le" or "sd" */
    int			(*pi_probe)();
    int			(*pi_boot)();
    int			(*pi_open)();
    int			(*pi_close)();
    int			(*pi_strat)();
    caddr_t		pi_desc;		/* Printable device name */
};


typedef struct prom_bootparms prom_bootparms_t;
struct prom_bootparms {
    caddr_t		pb_argv[8];		/* String arguments */
    char		pb_space[100];		/* Area for strings */
    char		pb_devname[2];		/* ie. "le" or "sd" */
    int			pb_cntrl;		/* Controller number */
    int			pb_unit;		/* Unit number */
    int			pb_part;		/* Partition number */
    caddr_t		pb_file;		/* File name (in pb_space) */
    prom_iodev_t *	pb_iodev;		/* Prom entry points */
};

typedef	struct prom_memory prom_memory_t;
struct prom_memory {
    prom_memory_t *	pm_next;		/* Next block in the chain */
    unsigned int	pm_addr;		/* Address of this block */
    unsigned int	pm_len;			/* Length of this block */
};

/* end VERSION 1 only */


/*
 * For the list of physical memory returned by the available memory routines
 * the get it from the prom, one way or another.
 */
struct phys_mem {
    unsigned int	phm_addr;		/* Address of this block */
    unsigned int	phm_len;		/* Length of this block */
};


/*
 * When the mmu interface enquires about a device by name it needs to get back
 * information about the physical address, the virtual address - if already
 * mapped in by the prom and possibly other fields. This is most conveniently
 * passed around in a struct.
 */
typedef struct device_information	device_info;
struct device_information {
    dnode_t	di_nodeid;
    dev_reg_t	di_paddr;
    vir_bytes	di_vaddr;
};


/*
 * For finding and registering other devices we use a linked list of pointers
 * to nodes in the device tree.  See the routine obp_regnode() for details
 * of the use of the list.
 */
typedef struct nodelist	nodelist;
struct nodelist {
    nodelist *	l_prevp;
    dnode_t	l_curnode;
};


/*
 * The rest is a combination of what has appeared in various versions
 * of the openprom over the years in attempt to support all of them so
 * that Amoeba will run on all the Sparcstations.
 */

/*
 * The address of the OPENPROM device is handed to our entry point as
 * its first argument (%o0). The only things we need are the memory map,
 * the argument strings, and the console printf function.
 *
 * Note that if the name of the field begins with v1_ then it only appears
 * in version 1 of the openprom and is "reserved" in subsequent versions.
 * If a name begins with v2_ then the operator in question only appears in
 * version 2 or earlier.
 * Things described in the draft standard begin with op.  Not all of them
 * are present in version 1 or version 2 of the openprom.  Comments are
 * used to mark entries not available in all versions and not already
 * identified by v1 or v2.
 */

/* The magic number that must be in the pd_magic field below */
#define	OBP_MAGIC	0x10010407

struct openboot {
    /* 0x00 */
    /* Version numbers */
    uint_t		op_magic;		/* Magic number */
    uint_t		op_romvec_version;	/* ROM vector version */
    uint_t		op_plugin_version;	/* Arch version */
    uint_t		op_mon_id;		/* PROM Monitor version */

    /* 0x10 */
    /* Memory chains */
    prom_memory_t **	v1_physmem;		/* Chain of all phys mem */
    prom_memory_t **	v1_virtmem;		/* Chain of used virtual mem */
    prom_memory_t **	v1_availmem;		/* Chain of unused phys mem */

    /* 0x1C */
    struct config_ops *	op_config_ops;		/* Device tree information */

    /* Boot information */
    char **		v1_bootcmd;		/* Expanded by prom options */

    /* Ancient I/O routines */
    uint_t		(*v1_open)();
    uint_t		(*v1_close)();
    uint_t		(*v1_blkread)();
    uint_t		(*v1_blkwrite)();
    uint_t		(*v1_pxmit)();
    uint_t		(*v1_precv)();
    uint_t		(*v1_bread)();
    uint_t		(*v1_bwrite)();
    uint_t		(*v1_seek)();

    /* Console I/O routines */
#define		OP_IKEYBD	0
#define		OP_IUARTA	1
#define		OP_IUARTB	2
    unsigned char	*v1_input;		/* Input source */

#define		OP_OVIDEO	0
#define		OP_OUARTA	1
#define		OP_OUARTB	2
    unsigned char *	v1_output;		/* Output destination */
    unsigned char	(*v1_getc)();		/* Getc( stdin ); */
    void		(*v1_putc)();		/* Putc( stdin ); */
    int			(*v1_getp)();		/* -1 ==> no c to get */
    int			(*v1_putp)();		/* -1 ==> no can put c */

    /* Frame buffer mangulation */
    void		(*v1_putstr)();		/* Write to framebuffer */

    /* 0x64 */
    void		(*op_boot)(/* caddr_t */);	/* Reboot machine */
    int			(*v1_printf)();		/* With max 6 args */
    /* 0x6C */
    void		(*op_enter)(/* void */);/* Entry for kbd abort */
    int *		op_milliseconds;	/* Time in ms */
    void		(*op_exit)(/* void */);	/* Return to monitor */
    void		(**op_callback)(/* void */);/* Handle vector commands */
    uint_t		(*op_interpret)(/* caddr_t */);/* Eat a forth string */
    /* 0x80 */
    prom_bootparms_t **	v1_parms;		/* New style boot parameters */
    unsigned int	(*v1_eaddr)();		/* Get EtherNet address */

    /*
     * Not available before version 2
     ********************************
     */
    /* 0x88 */
    char **		op_bootpath;
    char **		op_bootargs;

    ihandle_t *		op_stdin;		/* Console input device */
    ihandle_t *		op_stdout;		/* Console output device */
    /* 0x98 */
    phandle_t		(*op_phandle)(/* ihandle_t */);	/* Convert ihandle to phandle */
    caddr_t		(*op_alloc)(/* c_addr_t, uint_t */);	/* Allocate physical memory */
    void		(*op_free)(/* c_addr_t, uint_t */);	/* Dealloc physical memory */

    caddr_t		(*op_map)(/* c_addr_t, uint_t, uint_t, uint_t */);	/* Create device mapping */
    caddr_t		(*op_unmap)(/* c_addr_t, uint_t */);	/* Destroy device mapping */

    /* 0xAC */
    ihandle_t		(*op_open)(/* char * */);
    int			(*op_close)(/* ihandle_t */);
    int			(*op_read)(/* ihandle_t, caddr_t, uint_t */);
    int			(*op_write)(/* ihandle_t, caddr_t, uint_t */);
    int			(*op_seek)(/* ihandle_t, caddr_t, uint_t */);

    void		(*op_chain)(/* caddr_t, uint_t, caddr_t, caddr_t, uint_t */);
    void		(*op_release)(/* caddr_t, uint_t */);

    /* 0xC8 */
    int			op_reserved[15];

    /*
     * End section not available before version 2
     ********************************************
     */

    /* 0x104 */
    /*
     * (*promp->pd_setseg)( int context, int virtual_address, int pmeg );
     */
    void		(*v2_setseg)();		/* Set segment in other ctx */

    /*
     * Not available before version 3
     ********************************
     */

    /* 0x108 */
    /*
     * For multi-processor systems
     * Never *ever* call these when running on a uniprocessor!
     */
    int			(*op_startcpu)(/* dnode_t, dev_reg_t, int, addr_t */);
    int			(*op_stopcpu)(/* dnode_t */);
    int			(*op_idlecpu)(/* dnode_t */);
    int			(*op_resumecpu)(/* dnode_t */);
};

extern struct openboot *	prom_devp;	/* Our pointer to the PROM */

/*
 * Routines supported by openprom code
 */
void	obp_regnode _ARGS(( char *, char *, void (*cfunc)( nodelist * ) ));
void	obp_devinit _ARGS(( char * ));
void	obp_devaddr _ARGS(( nodelist *, dev_reg_t * ));
int	obp_availmem _ARGS(( int, struct phys_mem * ));
int	obp_getattr _ARGS(( dnode_t, char *, void *, int ));
char *	obp_genname _ARGS(( nodelist *, char *, char * ));

#endif	/* __OPENPROM_H__ */
