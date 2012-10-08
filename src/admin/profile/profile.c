/*	@(#)profile.c	1.6	96/02/27 10:17:32 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
profile.c
*/

#define _POSIX2_SOURCE 1

#if !defined(AMOEBA)
#	define __EXTENSIONS__ 1 /* for solaris to define popen */
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <amoeba.h>
#include <ampolicy.h>
#include <amtools.h>
#include <cmdreg.h>
#include <stderr.h>

#include <module/buffers.h>
#include <module/systask.h>
#include <module/host.h>

#define SZPROFBUF 	30000

#ifndef KERNMAXADDR
#define KERNMAXADDR 0x80000 /* Should be calculated from the last text symbol */
#endif

char *names[ KERNMAXADDR ];
long first_addr;
long addr_inited;

/* #define RNDADDR( addr ) ( addr & ~( sizeof( char * ) - 1 )) */
#define RNDADDR( addr ) (addr) 
/* Changed because ack doesn't align on 4-byte boundaries */

#if __STDC__
#define CONST const
#else
#define CONST
#endif

typedef struct node {
	char *n_key;
	long n_count;
	long n_time;
	int n_nchildren;
	int *n_children;
	int n_printed;
} node_t;

int verbose;
char *progname;
int use_anm;
node_t *nodelist;
int node_nr, node_max_nr;
int merge_nodes;

void main _ARGS(( int argc, char *argv[] ));
void usage _ARGS(( void ));
int processtree _ARGS(( struct node *n ));
void printtree _ARGS(( struct node *n, int depth ));
int compar _ARGS(( const void *n1, const void *n2 ));
char *buildtree _ARGS(( char *p, char *end, int *node ));
int addnode _ARGS(( int n, char *k ));
char *getsym _ARGS(( long addr ));
void readsym _ARGS(( char *aout ));
void append _ARGS(( long addr, char *name ));
int new_node _ARGS(( char *k ));
void init_node _ARGS(( node_t *node, char *k ));

void usage()
{
	fprintf(stderr, 
"Usage: %s [-a] [-i <interval>] [-k <kernel>] [-m] [-n <iterations>] machine\n",
			progname);
	exit(1);
}

void append(addr, name)
long addr;
char *name;
{
	char *s;

	if ( !addr_inited ) {
		addr_inited = 1;
		first_addr = addr;
		if ( verbose )
			fprintf( stderr, "kernel starts at %08x (%s)\n", 
								addr, name );
	}

	addr -= first_addr;
	if ( addr < 0 || addr >= KERNMAXADDR ) {
		if ( verbose )
			fprintf( stderr, "%s: %s(%08x): address out of range\n",
				progname, name, addr );
		return;
	}
	if ( addr != RNDADDR( addr )) {
		fprintf( stderr, "%s: %s(%08x): unaligned address\n",
			progname, name, addr );
		return;
	}
	if (*name == 'L')
		return;	/* Local labels in assembler files. */
	if (*name == '_')
		name++;
	if (*name == 0)
		return;
	if (verbose >= 3)
		printf("%s %d: calling malloc %d\n", __FILE__, __LINE__, 
			strlen(name) + 1);
	if ((s = (char *) malloc(strlen(name) + 1)) == 0) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	if (verbose >= 2)
		printf("adding symbol 0x%x, %s\n", addr, name);
	(void) strcpy(s, name);
	names[addr] = s;
}

void readsym(aout)
char *aout;
{
	char buf[256];
	FILE *fp;
	long addr;
	char *nl_cmd;

	if (use_anm)
		nl_cmd=
  "anm -n %s | sed -n 's/\\([^ ]*\\)  0  [E-] \\(.*\\)$/\\1 \\2/p'";
	else
		nl_cmd=
  "nm -n %s | sed -n 's/\\([^ ]*\\) [tT] \\([a-zA-Z0-9_]*\\)$/\\1 \\2/p'";
	sprintf(buf, nl_cmd, aout);
	if ((fp = popen(buf, "r")) == 0) {
		fprintf(stderr, "can't popen %s\n", buf);
		exit(1);
	}
	while (fscanf(fp, "%x %s", &addr, buf) == 2)
		append(addr, buf);
}

char *getsym(addr)
long addr;
{
	register char **p;

	addr -= first_addr;
	addr = RNDADDR( addr );
	if ( addr < 0 || addr >= KERNMAXADDR ) {
		if ( verbose )
			fprintf( stderr, "%s: %08x: address out of range\n",
				progname, addr );
		return "unknown";
	} else {
		for (p = &names[addr]; p > names; --p)
			if (*p != 0) {
				return *p;
			}
		return ".start";
	}
}

int addnode(n, k)
int n;
char *k;
{
	struct node *np, *kp;
	int *cl;
	register i;

	np= &nodelist[n];
	cl= np->n_children;
	for (i= 0; i<np->n_nchildren; i++)
	{
		kp= &nodelist[cl[i]];
		if (kp->n_key == k)
			break;
	}
	if (i >= np->n_nchildren)
	{
		np->n_nchildren++;
		if (!cl)
		{
			cl= malloc(np->n_nchildren * sizeof(int));
		}
		else
		{
			if (verbose >= 3)
				printf("%s %d: calling realloc %d\n", 
					__FILE__, __LINE__, np->n_nchildren * 
					sizeof(int));
			cl= realloc(cl, np->n_nchildren * sizeof(int));
						/* one extra node number */
		}
		if (!cl)
		{
			fprintf(stderr, "%s: out of memory\n", progname);
			exit(1);
		}
		cl[np->n_nchildren-1]= new_node(k);
		np= &nodelist[n];	/* re-evaluate np */
		kp= &nodelist[cl[np->n_nchildren-1]];
		np->n_children= cl;
	}
	kp->n_count++;
	return kp-nodelist;
}

char *buildtree(p, end, node)
char *p;
char *end;
int *node;
{
	uint32 pc;
	int n;
	char *k;

	assert(node);
	p= buf_get_uint32(p, end, &pc);
	if (!p)
		return NULL;

	if (!pc)
	{
		*node= 0;				/* root node */
		return p;
	}
	p= buildtree(p, end, &n);
	if (!p)
		return NULL;
	k= getsym(pc);
	n= addnode(n, k);
	*node= n;
	return p;
}

int compar(n1, n2)
CONST register void *n1, *n2;
{
	return nodelist[*(int *)n2].n_count - nodelist[*(int *)n1].n_count;
}

void printtree(n, depth)
register struct node *n;
int depth;
{
	register i = n->n_nchildren;
	int *c = n->n_children;
	struct node *cp;
	int j;

	assert(!n->n_printed);
	n->n_printed= 1;
	if (i == 0)
		return;
	qsort((_VOIDSTAR) c, (size_t) i, sizeof(int), compar);
	for (i = 0; i < n->n_nchildren; i++, c++) {
		cp= &nodelist[*c];
		for (j= 0; j<depth; j++)
			printf(".   ");
		printf("%ld(%ld)%s %.32s\n", cp->n_count, cp->n_time, 
			cp->n_printed ? "*" : "", cp->n_key);
		if (!cp->n_printed)
			printtree(cp, depth + 1);
	}
}


int processtree(n)
struct node *n;
{
	register i = n->n_nchildren;
	int *c = n->n_children;
	int r = 0;

	if (i == 0) {
		n->n_time = n->n_count;
		return n->n_count;
	}
	for (i = 0; i < n->n_nchildren; i++, c++) {
		r += processtree(&nodelist[*c]);
	}
	n->n_time = n->n_count - r;
	return n->n_count;
}


void main(argc, argv)
int argc;
char *argv[];
{
	char *machine;
	static char prof[SZPROFBUF];
	capability cap, host_cap;
	char *p, *last;
	errstat	error;
	int i, n;
	unsigned long interv;
	int n_iter;
	bufsize nbytes;
	int a_flag, m_flag, v_flag, c;
	char *i_arg, *k_arg, *n_arg, *check;
	char *kernel;

	progname = argv[0];
"Usage: %s [-a] [-i <interval>] [-k <kernel>] [-n <iterations>] [-v] machine\n",

	a_flag= 0;
	i_arg= NULL;
	k_arg= NULL;
	m_flag= 0;
	n_arg= NULL;
	v_flag= 0;

	while ((c= getopt(argc, argv, "?ai:k:mn:v")) != -1)
	{
		switch(c)
		{
		case 'a':
			if (a_flag)
				usage();
			a_flag= 1;
			break;
		case 'i':
			if (i_arg)
				usage();
			i_arg= optarg;
			break;
		case 'k':
			if (k_arg)
				usage();
			k_arg= optarg;
			break;
		case 'm':
			if (m_flag)
				usage();
			m_flag= 1;
			break;
		case 'n':
			if (n_arg)
				usage();
			n_arg= optarg;
			break;
		case 'v':
			v_flag++;
			break;
		case '?':
			usage();
		default:
			fprintf(stderr, "%s: getopt failed\n", progname);
			exit(1);
		}
	}

	if (optind >= argc)
		usage();
	machine= argv[optind++];
	if (optind != argc)
		usage();
	
	use_anm= a_flag;
	if (i_arg)
	{
		interv= strtol(i_arg, &check, 0);
		if (*check)
		{
			fprintf(stderr, "%s: illegal interval: '%s'\n",
				progname, i_arg);
			exit(1);
		}
	}
	else
		interv= 10;
	if (k_arg)
		kernel= k_arg;
	else
		kernel= "kernel";
	merge_nodes= m_flag;
	if (n_arg)
	{
		n_iter= strtol(n_arg, &check, 0);
		if (*check)
		{
			fprintf(stderr, 
				"%s: illegal number of iterations: '%s'\n",
					progname, n_arg);
			exit(1);
		}
	}
	else
		n_iter= 1;
	
	verbose= v_flag;

	error= super_host_lookup(machine, &host_cap);
	if (error != STD_OK)
	{
		fprintf(stderr, "%s: can't get capability for '%s': %s\n", 
			progname, machine, err_why(error));
		exit(1);
	}
	error= dir_lookup(&host_cap, DEF_PROFILENAME, &cap);
	if (error != STD_OK)
	{
		fprintf(stderr, "%s: can't lookup profile server: %s\n",
						progname, err_why(error));
		exit(1);
	}
	readsym(kernel);
	node_nr= 1;
	node_max_nr= 1;
	if (verbose >= 3)
		printf("%s %d: calling calloc %d, %d\n", __FILE__, __LINE__, 
			node_max_nr, sizeof(node_t));
	nodelist= calloc(node_max_nr, sizeof(node_t));
	if (!nodelist)
	{
		fprintf(stderr, "%s: out of memory\n", progname);
		exit(1);
	}
	init_node(&nodelist[0], NULL);
	for (i= 0; i<n_iter; i++)
	{
		nbytes = sys_profile(&cap, prof, SZPROFBUF, interv);
		if(ERR_STATUS(nbytes)) 
		{
			fprintf(stderr, "%s: failed: %s\n", progname, 
						err_why(ERR_CONVERT(nbytes)));
			exit(1);
		}
		if (verbose)
			fprintf(stderr, "profile buffer is %d bytes\n",
				nbytes);

		last = (prof + nbytes);
		p= prof;
		while (p)
		{
			p= buildtree(p, last, &n);
			if (p)
				nodelist[n].n_time++;
		}
	}
	for (i= 0; i< node_nr; i++)
		nodelist[i].n_printed= 0;
	printtree(&nodelist[0], 0);
	exit(0);
}

void init_node(node, k)
node_t *node;
char *k;
{
	node->n_key= k;
	node->n_count= 0;
	node->n_time= 0;
	node->n_nchildren= 0;
	node->n_printed= 0;
	node->n_children= 0;
}

int new_node(k)
char *k;
{
	int i;

	if (merge_nodes)
	{
		for (i= 0; i< node_nr; i++)
			if (nodelist[i].n_key == k)
			{
				if (verbose >= 2)
					printf("new_node('%s')= %d\n", k, i);
				return i;
			}
	}
	if (node_nr >= node_max_nr)
	{
		node_max_nr *= 2;
		if (verbose)printf("%s %d: calling realloc %d\n", __FILE__, 
			__LINE__, node_max_nr, node_max_nr * sizeof(node_t));
		nodelist= realloc(nodelist, node_max_nr * sizeof(node_t));
		if (!nodelist)
		{
			fprintf(stderr, "%s: out of memory\n", progname);
			exit(1);
		}
	}
	init_node(&nodelist[node_nr], k);
	if (verbose >= 2)
		printf("new_node('%s')= %d\n", k, node_nr);
	return node_nr++;
}
