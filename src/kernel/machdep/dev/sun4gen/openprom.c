/*	@(#)openprom.c	1.4	96/02/27 13:55:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "string.h"
#include "debug.h"
#include "machdep.h"
#include "openprom.h"
#include "sys/proto.h"
#include "assert.h"
INIT_ASSERT


/* Routines to get the device tree information */

#define	PROM_NEXTNODE(id) \
	(dnode_t) (*prom_devp->op_config_ops->dp_next)(id)
#define	PROM_CHILDNODE(id) \
	(dnode_t) (*prom_devp->op_config_ops->dp_child)(id)
#define	PROM_GETPROPLEN(id, name) \
	(*prom_devp->op_config_ops->dp_gplen)(id, name)
#define	PROM_GETPROP(id, name, value) \
	(*prom_devp->op_config_ops->dp_getprop)(id, name, value)
#define	PROM_NEXTPROP(id, previous) \
	(char *) (*prom_devp->op_config_ops->dp_nxtprop)(id, previous)

/*
 * prom_getroot
 *	Look up the root of the device tree and return its nodeid.
 *	Don't return if it isn't there.
 */

static dnode_t
prom_getroot()
{
    dnode_t	nodeid;

    nodeid = PROM_NEXTNODE(0);
    if (nodeid == NONODE || nodeid == BADNODE)
    {
	panic("Can't find device tree for this system");
    }
    return nodeid;
}


/*
 * prom_matchnode
 *	Boolean function that checks if the specified node has "name"
 *	attribute equal to name.  Return 1 if is so.  Otherwise return 0.
 */

static int
prom_matchnode(node, name)
dnode_t	node;	/* node for our test */
caddr_t	name;	/* name of the node we wish to find */
{
    int		len;
    char	buf[MAXDEVNAME];
    char *	attribute = "name";

    len = PROM_GETPROPLEN(node, attribute);
    if (len <= 0 || len >= MAXDEVNAME)
	return 0;
    (void) PROM_GETPROP(node, attribute, buf);
    return !strcmp(name, buf);
}


/*
 * prom_findnode
 *	Search the device tree for the named node, starting at "curnode".
 *	Returns NONODE if it found no matching node.  Otherwise it returns
 *	the node identifier of the node that matched.
 */

static dnode_t
prom_findnode(curnode, name)
dnode_t	curnode;	/* starting node for our search */
caddr_t	name;		/* name of the node we wish to find */
{
    dnode_t	newnode;

    if (prom_matchnode(curnode, name))
	return curnode;

    /*
     * Search breadth first since the device tree is quite flat and mostly
     * what we want is at the top level.
     */
    newnode = PROM_NEXTNODE(curnode);
    if (newnode != BADNODE && newnode != NONODE &&
			(newnode = prom_findnode(newnode, name)) != NONODE)
    {
	return newnode;
    }

    newnode = PROM_CHILDNODE(curnode);
    if (newnode != BADNODE && newnode != NONODE &&
			(newnode = prom_findnode(newnode, name)) != NONODE)
    {
	return newnode;
    }

    return NONODE;
}


struct range_info {
	uint_t	child_high;
	uint_t	child_low;
	uint_t	parent_high;
	uint_t	parent_low;
	uint_t	size;
};
typedef struct range_info rng_info_t;

static void
prom_only_backtrack(node, reg_info)
nodelist *	node;
dev_reg_t *	reg_info;
{
    char *	regattr = "reg";
    char *	rangeattr = "ranges";
    char *	slotbitsattr = "slot-address-bits";
    int		i;
    int		proplen;
    rng_info_t	range[8];  /* We don't expect more ranges than this */
    static dev_reg_t null_info;	/* to return if things go wrong */


    DPRINTF(1, ("starting backtrack\n"));
    while (node->l_prevp)
    {
	DPRINTF(1, ("curnode = %x\n", node->l_curnode));
	node = node->l_prevp;
	proplen = PROM_GETPROPLEN(node->l_curnode, rangeattr);
	if (proplen > 0)
	{
	    assert((proplen % sizeof (rng_info_t)) == 0);
	    proplen /= sizeof (rng_info_t);
	    assert(proplen <= 8);
	    if (PROM_GETPROP(node->l_curnode, rangeattr, range) < 0)
	    {
		DPRINTF(1,
    ("prom_backtrack: node %x has %d bytes of range info but getprop failed\n",
			node->l_curnode, proplen));
		*reg_info = null_info;
		return;
	    }
	    for (i = 0; i < proplen; i++)
	    {
		DPRINTF(1, ("prom_backtrack: range %x %x %x %x %x\n",
				range[i].child_high, range[i].child_low,
				range[i].parent_high, range[i].parent_low,
				range[i].size));

		if (reg_info->reg_bustype == range[i].child_high &&
		    reg_info->reg_addr >= range[i].child_low &&
		    reg_info->reg_addr + reg_info->reg_size <=
			range[i].child_low + range[i].size)
		{
		    /*
		     * We've found the right range for our device!
		     * Strictly speaking we should do 64-bit arithmetic but
		     * it isn't really worth it.
		     */
		    reg_info->reg_addr += range[i].parent_low - range[i].child_low;
		    reg_info->reg_bustype = range[i].parent_high;
		    break;
		}
	    }
	}
	else
	{
	    /*
	     * The PROM doesn't have a range field
	     */

	    DPRINTF(1,
		("prom_backtrack: no range field - looking at slot bits\n"));
	    proplen = PROM_GETPROPLEN(node->l_curnode, slotbitsattr);
	    if (proplen > 0)
	    {
		/* It has an old version of the prom with only the
		 * slot-address-bits so we must assume that the sbus slots'
		 * address ranges are contiguous with each other.  !@#$%
		 */
		dev_reg_t	sbus_info;
		uint_t		slotbits;

		if (PROM_GETPROP(node->l_curnode, slotbitsattr, &slotbits) < 0)
		{
		    panic("prom_backtrack: can't get slot-address-bits");
		}
		proplen = PROM_GETPROPLEN(node->l_curnode, regattr);
		assert(proplen == sizeof (dev_reg_t));
		if (PROM_GETPROP(node->l_curnode, regattr, &sbus_info) < 0)
		{
		    panic("prom_backtrack: can't get sbus register info");
		}
		reg_info->reg_addr += sbus_info.reg_addr +
					reg_info->reg_bustype * (1 << slotbits);
		compare(reg_info->reg_addr, <=, sbus_info.reg_addr - 1 + sbus_info.reg_size);
		reg_info->reg_bustype = sbus_info.reg_bustype;

	    }
	}
    }
    DPRINTF(1, ("prom_backtrack: done: %x %x %x\n", reg_info->reg_bustype,
		reg_info->reg_addr, reg_info->reg_size));
    return;
}

static void
prom_backtrack(node, reg_info)
nodelist *	node;
dev_reg_t *	reg_info;  /* We don't accept nodes with more than 1 reg set */
{
    char *	regattr = "reg";
    int		proplen;
    static dev_reg_t null_info;	/* to return if things go wrong */

    proplen = PROM_GETPROPLEN(node->l_curnode, regattr);
    if (proplen != sizeof (dev_reg_t) ||
			PROM_GETPROP(node->l_curnode, regattr, reg_info) < 0)
    {
	*reg_info = null_info;
	return;
    }
    prom_only_backtrack(node, reg_info);
}


/*
 * prom_pathsearch
 *	Recursively search for all nodes matching the pathname "name" from
 *	the root "node".  This is used to implement obp_findnode.
 *	 NB. This code is not reentrant.  It relies on the state of the PROM
 *	     for doing repeated "nextchild" calls for the same node, for
 *	     example.
 */

static void
prom_pathsearch(node, name, nodeids, size, matches)
nodelist *	node;		/* root from which to begin the search */
char *		name;		/* pathname to search for */
device_info *	nodeids;	/* array to be returned to caller */
int		size;		/* size of nodeids array */
int *		matches;	/* number of matching nodes found */
{
    char *	endp;
    dnode_t	newnode;
    nodelist	local;
    int		count;


    /* Strip off multiple '/' characters */
    while (*name == '/')
	name++;
    
    if (*name != '\0')
    {
	/*
	 * If the pathname is not yet null then we take the next component of it
	 * and see if it exists.  Note we make endp point to a null string if
	 * there are no more slashes - it should simply point to the end of the
	 * "name" string if there are no more slashes but there is no need to
	 * waste time looking for the end.
	 */
	endp = strchr(name, '/');
	if (endp != (char *) 0)
	    *endp++ = '\0';
	else
	    endp = ""; /* Last component in the name has been found */

	/*
	 * We haven't reached the end of the pathname.
	 * Drop down to next depth of child.
	 * There can be more than one directory or leaf-node with the
	 * same name!!  Therefore we must repeatedly attempt the match
	 * at the same level in the tree, even if we've already had a match
	 * at this level.
	 */
	local.l_prevp = node;
	newnode = PROM_CHILDNODE(node->l_curnode);
	if (newnode != NONODE && newnode != BADNODE)
	{
	    do
	    {
		if (prom_matchnode(newnode, name))
		{
		    DPRINTF(2, ("Matched '%s'\n", name));
		    local.l_curnode = newnode;
		    prom_pathsearch(&local, endp, nodeids, size, matches);
		}
		newnode = PROM_NEXTNODE(newnode);
	    } while (newnode != NONODE && newnode != BADNODE);
	}
	return;
    }

    /* We have reached a matching node! */
    count = *matches;
    (*matches)++;
    if (count < size)
    {
	nodeids += count;
	nodeids->di_nodeid = node->l_curnode;
	if (obp_getattr(nodeids->di_nodeid, "address",
			(void *) &nodeids->di_vaddr, sizeof (vir_bytes))
			!= sizeof (vir_bytes))
	{
	    nodeids->di_vaddr = 0;
	    DPRINTF(2, ("prom_pathsearch: no address attribute for node %x\n",
			nodeids->di_nodeid));
	}
	prom_backtrack(node, &nodeids->di_paddr);
    }
    return;
}


/*
 * prom_regnode
 *	Does the work of the obp_regnode.
 *	NB. This code is not reentrant - it uses a static buffer for fetching
 *	    the value of the attribute.
 */
void
prom_regnode(attr, value, cfunc, currlist)
char *		attr;
char *		value;
void		(*cfunc) _ARGS((nodelist *));
nodelist *	currlist;
{
    dnode_t	newnode;
    dnode_t	curnode;
    int		len;
    nodelist	locallist;
    static char	valuebuf[128];

    /* First fill in details about current node and then go down a level */
    curnode = currlist->l_curnode;

    len = PROM_GETPROPLEN(curnode, attr);
    if (len > 0 && len < 128)
    {
	/* Attribute is present - let's check it out */
	(void) PROM_GETPROP(curnode, attr, valuebuf);

	/*
	 * If the attribute matches the one we are interested in then
	 * call cfunc
	 */
	if (strcmp(value, valuebuf) == 0)
	{
	    (*cfunc)(currlist);
	}
    }

    /*
     * Search depth first since the device tree is quite flat and mostly
     * what we want is at the top level.
     */
    newnode = PROM_CHILDNODE(curnode);
    if (newnode != BADNODE && newnode != NONODE)
    {
	locallist.l_prevp = currlist;
	locallist.l_curnode = newnode;
	prom_regnode(attr, value, cfunc, &locallist);
    }

    /*
     * Now we look at sibling node.
     */
    newnode = PROM_NEXTNODE(curnode);
    if (newnode != BADNODE && newnode != NONODE)
    {
	currlist->l_curnode = newnode;
	prom_regnode(attr, value, cfunc, currlist);
    }
}


/*
 **************************************************************************
 *	External routines for getting info out of the openprom and into	  *
 *	a form we can use.						  *
 **************************************************************************
 */


/*
 * obp_getattr
 *
 *	Read attribute "attr" from the specified node.  If the node is zero
 *	then use the root node.
 *	Return -1 if the node was not valid or if the property was not present.
 *	Otherwise return the number of bytes found in the attribute.
 *	Note that if the return buffer was too small to hold the property
 *	data then the data will not be looked up and set in *value.  The
 *	actual size will still be returned though to allow for the caller to
 *	try with a bigger buffer.
 */

int
obp_getattr(nodeid, attr, value, size)
dnode_t	nodeid;	/* Node in which property can be looked up */
char *	attr;	/* Name of the property to look up */
void *	value;	/* Buffer to receive the contents of the property */
int	size;	/* Size of buffer in bytes */
{
    int		proplen;

    DPRINTF(3, ("obp_getattr: lookup %s\n", attr));
    if (nodeid == 0)
	nodeid = prom_getroot();

    if (nodeid == NONODE || nodeid == BADNODE)
	return -1;

    proplen = PROM_GETPROPLEN(nodeid, attr);
    DPRINTF(2, ("obp_getattr: prom_getproplen returned %d\n", proplen));
    if (proplen > 0 && proplen <= size)
    {
	if (PROM_GETPROP(nodeid, attr, value) < 0)
	{
	    DPRINTF(2, ("obp_getattr: prom_getprop failed\n"));
	    return -1;
	}
    }
    return proplen;
}


/*
 * obp_availmem
 *
 *	Get the details of the available memory on the sparc station.
 *	The arguments are an array of {address, size} pairs for the memory
 *	blocks and the length of the array.  It returns the number of entries
 *	that were filled in in the array.  Zero implies catastrophic error -
 *	namely no memory could be found.
 */

int
obp_availmem(len, memblocks)
int		  len;	/* length of array memblocks */
struct phys_mem * memblocks;
{
    dnode_t	nodeid;

    /* See if there is a memory node */
    nodeid = prom_findnode(prom_getroot(), "memory");
    if (nodeid == NONODE || nodeid == BADNODE)
    {
	prom_memory_t * pmp;
	int		i;

	DPRINTF(2, ("There was no memory node in the device tree\n"));
	/*
	 * While there is still room in the memblocks array and
	 * there are still entries in the linked list of memory
	 * chunks we fill the memblocks array
	 */
	for (i = 0, pmp = *prom_devp->v1_availmem;
			i < len && pmp; i++, pmp = pmp->pm_next)
	{
	    memblocks->phm_addr = pmp->pm_addr;
	    memblocks->phm_len = pmp->pm_len;
	    memblocks++;
	}
	return i;
    }
    else
    {
	/*
	 * The memory attribute returns a set of structs, one for each
	 * available chunk.  We pray it is never more than twenty.
	 */
	static struct meminfo {
	    uint_t	high;
	    uint_t	low;
	    uint_t	size;
	}	mem_info[20];
	int	proplen;
	int	count;
	char *	availattr = "available";

	if ((proplen = PROM_GETPROPLEN(nodeid, availattr)) <= 0)
	{
	    DPRINTF(2,
		    ("available attribute for memory returned %d\n", proplen));
	    return 0;
	}
	proplen /= sizeof (struct meminfo);
	DPRINTF(0, ("available has %d chunks\n", proplen));
	compare(proplen, <=, sizeof mem_info);
	if (PROM_GETPROP(nodeid, availattr, (addr_t) mem_info) <= 0)
	{
	    printf("WARNING: prom_getprop for available memory failed\n");
	    return 0;
	}
	/*
	 * If the caller has insufficient space for all the chunks
	 * only tell as much as there is room for
	 */
	if (proplen > len)
	{
	    printf("WARNING: not all available memory was reported\n");
	    printf("%d chunks available. Client buflen is %d\n", proplen, len);
	    proplen = len;
	}

	/*
	 * The chunks, as returned to us are in reverse order so we
	 * put them into the callers buffer in the reverse order from
	 * that returned by the prom.
	 */
	count = proplen;
	while (--count >= 0)
	{
	    memblocks->phm_addr = mem_info[count].low;
	    memblocks->phm_len  = mem_info[count].size;
	    memblocks++;
	}
	return proplen;
    }
}


#if 1
/*
 * obp_findnode
 *
 *	Find the nodes specified by the name arg and return their node
 *	identifiers in devinfo.  If the caller's array is too small for
 *	all nodeids then fill it with all it can take.  The value of the
 *	function is the number of nodes actually in the system with that
 *	name - possibly more than the number returned in the caller's array.
 */

int
obp_findnode(name, devinfo, numnodes)
char *		name;
device_info *	devinfo;
int		numnodes;
{
    int		nodecount = 0;
    char	namebuf[MAXDEVNAME];
    nodelist	head;


    DPRINTF(1, ("obp_findnode(%s, %x, %d)\n", name, devinfo, numnodes));
    /* The pathname may be in readonly data so it must be copied */
    (void) strcpy(namebuf, name);

    /*
     * Head of backpointer list used to recover node info from previous
     * nodes in the path.  This is needed to reconstruct the physical
     * address in the case of certain devices such as sbus devices.
     */

    head.l_curnode = prom_getroot();
    head.l_prevp = 0;

    prom_pathsearch(&head, namebuf, devinfo, numnodes, &nodecount);
    return nodecount;
}
#endif


/*
 * obp_regnode
 *
 *	This routine searches through the device tree for nodes that
 *	have the attribute "attr" equal to "value".  When it gets a match
 *	it calls the routine "cfunc" with the list of nodes which form
 *	the path in the device tree to that node.
 *
 * NB.	Only attributes whose type is char * can be compared with safety.
 *	It is too difficult to make a more generic routine and not necessary
 *	to make the sparc port work.
 *
 *	If you want to know how many devices are present so that you can then
 *	allocate resources, first pass a routine that increments a counter and
 *	then call this again with the function to do the real work.
 */

void
obp_regnode(attr, value, cfunc)
char *	attr;
char *	value;
void	(*cfunc) _ARGS((nodelist *));
{
    nodelist locallist; 

    /* Make a root for the linked list */
    locallist.l_prevp = 0;
    locallist.l_curnode = prom_getroot();

    /*
     * Run through the device tree and find every match of attribute attr with
     * value and call "cfunc" for each match.
     */
    prom_regnode(attr, value, cfunc, &locallist);
}


/*
 * obp_genname
 *	Given a nodelist, generate the full pathname represented by the list.
 *	Like all the other silly tree-walking stuff this is recursive too :-)
 *	Alas is returns the null string in the case of a nodelist of one
 *	element instead of '/'.
 */

char *
obp_genname(list, path, end)
nodelist *	list;	/* List from which to generate path */
char *		path;	/* Buffer in which to store the result */
char *		end;	/* Pointer to end of path buffer */
{
    int		proplen;
    dev_reg_t	reg_info;

    if (path == 0)
	return 0;

    /*
     * Go back up the list till we get to the beginning
     * But do not look up the name of the first node - it is /
     * and is covered by prepending a / to each segment of the path
     */
    if (list->l_prevp != 0)
    {
	if ((path = obp_genname(list->l_prevp, path, end)) == (char *) 0)
	    return 0;

	*path++ = '/';
	proplen = obp_getattr(list->l_curnode, "name", (void *) path,
				end - path);
	if (proplen < 0)
	    return 0;
	path += proplen - 1;

	/*
	 * See if there is a reg attribute - if so add it to the name
	 * of the device.
	 */
	proplen = obp_getattr(list->l_curnode, "reg", (void *) &reg_info,
				sizeof reg_info);
	if (proplen == sizeof (dev_reg_t))
	{
	    path = bprintf(path, end, "@%x,%x",
			   reg_info.reg_bustype, reg_info.reg_addr);
	    if (path == 0)
		return 0;
	}
    }
    *path = '\0';
    return path;
}


/*
 * obp_devinit
 *	Some devices don't set all their attributes until an open has been done
 *	on them.  Therefore if we find a node that matches the sort of device
 *	we are seeking then we need to be able to open it.
 */

void
obp_devinit(path)
char *	path;
{
    ihandle_t	fd;

    /* Open the device */
    if (prom_devp->op_romvec_version < 2)
	return; /* This function isn't needed for really old proms */

    fd = (*prom_devp->op_open)(path);
    if (fd == 0)
    {
	printf("obp_devopen: open of `%s' failed\n", path);
	return;
    }
}


int
obp_n_devaddr(p, d, n)
nodelist *	p;
dev_reg_t *	d;
int n;
{
    char *	regattr = "reg";
    int		proplen;
    int		nvals;
    int		i;

    proplen = PROM_GETPROPLEN(p->l_curnode, regattr);
    if (proplen % sizeof (dev_reg_t) != 0 ||
	proplen > n*sizeof (dev_reg_t) ||
			PROM_GETPROP(p->l_curnode, regattr, d) < 0)
    {
	return 0;
    }
    nvals = proplen / sizeof (dev_reg_t);
    for(i=0; i<nvals; i++)
	prom_only_backtrack(p, d+i);
    return nvals;
}

void
obp_devaddr(p, d)
nodelist *	p;
dev_reg_t *	d;
{
    static dev_reg_t null_info;	/* to return if things go wrong */

    if (obp_n_devaddr(p, d, 1) != 1)
	*d = null_info;
}
