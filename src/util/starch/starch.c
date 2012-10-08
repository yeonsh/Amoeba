/*	@(#)starch.c	1.6	96/02/27 13:16:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* STARCH - STore and reSTore ARCHives of graphs.

	STORE:
		starch -o [-f file] [-e] [-v] [-x path] [-X pattern]
			[-[Mm]] [-l] [-f] [-n n_iobufs]
			[-a file] [-A file]
			[-u types] [[-y types] [-z types]]
			[-U server-path ] [-Y server-path ]
			[-Z server-path ] name ...

	RESTORE:
		starch -i [-f file] [-r ] [-w] [-v] [-k] [-x path] [-X pattern]
			[-F file] [-[Mm]] [-l] [-f] [-n n_iobufs]
			[-B server-path ] [-D server-path ] 
			[name ...]

	LIST:
		starch -t [-f file] [-v] [-x path] [-X pattern] [name ...]
			[-F file] [-[Mm]] [-l] [-f] [-n n_iobufs]

	NAMES:
		starch -g [-x path] [-X pattern]
			[-u types] [[-y types] [-z types]]
			[-U server-path ] [-Y server-path ]
			[-Z server-path ] src

     --- undocumented ---
	PRIV2PUB
		starch -p [-f] [-v] [-x path] [-X pattern]
			[-u types] [[-y types] [-z types]]
			[-U server-path ] [-Y server-path ] [-Z server-path ]
			[-P server-path ] name ...

		

	-o			store archive
	-i			restore archive
	-t			list archive
	-c 			copy
	-a file			produce dump contents (with -o)
	-A file			read dump contents (with -o)
	-g			name generation
	-v			verbose
	-w			overwrite existing entries
	-r			retain existing entries
	-k			store capabilities unreachable due to
				tape errors on the tape.
	-e			dump whole capsets, do not assume that
			        capabilities in a capset are equivalent
	-f file			Use file indicated as archive
	-F file			The file describes the input files to use
	-m			Use magtape
	-M			Use magtape, do not rewind
	-l			Use floppy interface
	-s			Size of medium (max)
	-b			Size of write/read buffer ( %1024==0)
	-n			Number of 1K internal io buffers (80)
	-d			Debug
	-S			Produce statistics.
	-x path			do not dump/restore the capability under path
	-X pattern		do not dump/restore `pattern' file names
	-u string		Type of capabilities to dump (std_info)
	-U server-path		Dump `server' capability contents
	-y string		Types of capabilities to write as cap.
	-Y server-path		Dump `server' capabilities as caps.
	-z string		Types of capabilities to ignore
	-Z server-path		`Server' capabilities are ignored
	-B server-path		create files on server `server-path'
	-D server-path		create dirs on server `server-path'
	
*/

#include "starch.h"
#include <sp_dir.h>
#include <module/dgwalk.h>
#include <module/goodport.h>
#include <module/path.h>
#ifdef	MULTI_THREADED
#include <semaphore.h>
#include <module/mutex.h>
#include <thread.h>
#endif

/* I wanted to use cs_initial, but could not count on it */
#define INI_CS(cs)	0
/* #define INI_CS	((cs)->cs_initial) */

#ifdef __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif

/* Tuning of variables in other modules */
extern int		rw_nblocks ;
			
/* A few tunable program parameters */
char		*sts_columns[4] = { "owner", "group", "other" } ;
int		sts_ncols = 3 ;	/* # columns */
long		sts_rights[MAX_COLS] ;/* default rights */


/* Arguments */
char			*progname = "STARCH" ;
enum action		act = NONE ; 		/* o(A),i,t,c,g */
int			verbose ;	/* v */
int			debug ;		/* d */
int			equiv = 1 ;	/* e */
int			io_type ;	/* l & m & M */
int			force ;		/* f */
int			retain ;	/* r */
struct pathlist		*xlist ;	/* x */
struct patternlist	*xxlist ;	/* X */
char			*use_types ;	/* u */
char			*asis_types ;	/* y */
char			*ign_types ;	/* z */
struct serverlist	*use_servers ;	/* -U */
struct serverlist	*asis_servers ;	/* -Y */
struct serverlist	*ign_servers ;	/* -Z */
struct serverlist	*file_servers ;	/* -B */
struct serverlist	*priv_servers ;	/* -P */
struct serverlist	*dir_servers ;	/* -D */
char			*unreach_path ;	/* -k */
int			stats ;		/* -S */
#ifdef MULTI_THREADED
extern int		niobuf ;	/* -n */
#endif

struct serverlist	*arch_slist ;

struct pathlist		*files ;	/* file name arguments */

/* Variables derived from above */
int			def_method ;	/* Whether to ignore by default */

/* Memory allocation */
int			recursive ;
ptr			extramem ;

/* Output file for error messages */
FILE			*errfile= stderr ;

/* Files for conversing with user */
FILE			*i_talkfile= stdin ;
FILE			*o_talkfile= stderr ;

/* archive name admin */
char			*archive ;	/* file */
char			*ar_type ;	/* -A, -a or -F */
struct f_head_t		arch_list ;	/* List of '-f' archives */
struct f_head_t		ddi_list ;	/* List of '-A' archives */
struct f_head_t		ddo_list ;	/* List of '-a' archives */
int			in_multi ;	/* Multiple input archives  */

#ifdef MULTI_THREADED
/* Most write to stderr in this module will not use the lock mechanism,
   because they occur before any threads are started.
 */
static mutex		lck_errfile ;	/* mutex for err_file */
#endif

/* Some forward declarations */
void			f_cfd();
void			ini_slist();
void			tail_archive();
unsigned long		medm_sz();
void			decargs();
void			chk_st();
void			initargs();
void			initmem();
void			findmem();
void			soap_err();
void			bullet_err();


/* The main routine */

main(argc, argv)
	int	argc;
	char	**argv;
{
	int	notretval ;

	if ( sizeof(capability)!=CAP_SZ ) {
		fprintf(stderr,"warning: inconsistency in capability size\n") ;
		fprintf(stderr,"         are you running a new/old version of amoeba?\n" ) ;
	}
	initmem();
	decargs(argc,argv);
	initargs() ;
	sts_ncols=sp_mask(MAX_COLS,sts_rights);
	switch (act) {
	case IN:
		notretval= doget();
		break;
	case OUT:
		notretval= doput();
		break;
	case RESCAN:
		notretval= dorescan();
		break;
	case NAMES:
		errfile= stdout ;
		notretval= listnames();
		break;
	case LIST:
		errfile= stdout ;
		notretval= dolist();
		break;
	case PRIV:
		notretval= dopriv();
		break;
	default:
		fatal("Out of switch"); /* Should not be here */
	}
#ifdef MEMDEBUG
	main_cleanup();
#endif
	return !notretval ;
}	

void
decargs(argc, argv)
	int	argc;
	char	**argv;
{
	int 		c;
	extern char 	*optarg;
	extern int 	optind;
	char		*pn ;
	unsigned long	sz_buf=0 ;

	if ( argc<1 ) return ;

	pn=strrchr(argv[0],'/') ;	/* Determine program name */
	if ( pn && *(pn+1) ) progname=pn+1 ;
	else if ( argv[0][0] ) progname=argv[0] ;

	while ((c = getopt(argc, argv, "oitgevpk:a:A:f:lmMs:b:n:rwu:U:x:X:y:Y:z:Z:D:B:dSP:")) != -1)
		switch (c) {
		case 'o':
			if (act!=NONE)
				syntax();
			else
				act=OUT;
			break;
		case 'i':
			if (act!=NONE)
				syntax();
			else
				act=IN;
			break;
		case 't':
			if (act!=NONE)
				syntax();
			else
				act=LIST;
			break;
		case 'g':
			if (act!=NONE)
				syntax();
			else
				act=NAMES;
			break;
		case 'p':
			if (act!=NONE)
				syntax();
			else
				act=PRIV;
			break;
		case 'l':
			if ( io_type!=IO_FILE ) {
				fatal("can not combine -l, -m, -M") ;
			}
			io_type=IO_FLOP ;
			break ;
		case 'm':
			if ( io_type!=IO_FILE ) {
				fatal("can not combine -l, -m, -M") ;
			}
			io_type=IO_TAPE ;
			break ;
		case 'M':
			if ( io_type!=IO_FILE ) {
				fatal("can not combine -l, -m, -M") ;
			}
			io_type=IO_TAPE_NR ;
			break ;
		case 'f':
			f_insert(&arch_list,optarg,io_type);
			io_type=IO_FILE ;
			break ;
		case 'a':
			if ( ddo_list.first ) {
				fatal("Can allow only one -a");
			}
			f_insert(&ddo_list,optarg,io_type);
			io_type=IO_FILE ;
			break ;
		case 'A':
			if ( ddi_list.first ) {
				fatal("Can allow only one -A");
			}
			f_insert(&ddi_list,optarg,io_type);
			io_type=IO_FILE ;
			break ;
		case 's':
			sz_medium=medm_sz(optarg) ;
			break ;
		case 'b':
			sz_buf=medm_sz(optarg);
			break ;
		case 'n':
#ifdef MULTI_THREADED
			niobuf=atoi(optarg) ;
#else
			warning("-n option is only useful under amoeba");
#endif
			break ;
		case 'd':
			debug++;
			break;
		case 'S':
			stats++;
			break;
		case 'e':
			equiv=0;
			break;
		case 'v':
			verbose++;
			break;
		case 'w':
			force=1;
			break;
		case 'r':
			retain=1;
			break;
		case 'k':
			unreach_path=optarg ;
			break ;
		case 'u':
			use_types=optarg ;
			break ;
		case 'U':
			{	register struct serverlist *new ;
				new=MYMEM(struct serverlist);
				new->next= use_servers ;
				new->path= optarg ;
				use_servers=new ;
			}
			break;
		case 'x':
			{	register struct pathlist *new ;
				new=MYMEM(struct pathlist);
				new->next= xlist ;
				new->path= optarg ;
				xlist=new ;
			}
			break;
		case 'X':
			{	register struct patternlist *new ;
				new=MYMEM(struct patternlist);
				new->next= xxlist ;
				new->pattern= optarg ;
				xxlist=new ;
			}
			break;
		case 'y':
			asis_types=optarg ;
			break ;
		case 'Y':
			{	register struct serverlist *new ;
				new=MYMEM(struct serverlist);
				new->next= asis_servers ;
				new->path= optarg ;
				asis_servers=new ;
			}
			break;
		case 'z':
			ign_types=optarg ;
			break ;
		case 'Z':
			{	register struct serverlist *new ;
				new=MYMEM(struct serverlist);
				new->next= ign_servers ;
				new->path= optarg ;
				ign_servers=new ;
			}
			break;
		case 'D':
			{	register struct serverlist *new ;
				new=MYMEM(struct serverlist);
				new->next= dir_servers ;
				new->path= optarg ;
				dir_servers=new ;
			}
			break;
		case 'P':
			{	register struct serverlist *new ;
				new=MYMEM(struct serverlist);
				new->next= priv_servers ;
				new->path= optarg ;
				priv_servers=new ;
			}
			break;
		case 'B':
			{	register struct serverlist *new ;
				new=MYMEM(struct serverlist);
				new->next= file_servers ;
				new->path= optarg ;
				file_servers=new ;
			}
			break;
		case '?':
			syntax();
		}
	if ( force && retain ) syntax() ;
	if ( unreach_path ) {
		if ( act!=IN || xxlist || xlist ) {
			warning("-k only allowed with full restores");
			syntax() ;
		}
	}

	switch (act) {
	default:
	case NONE:
		(void) syntax();
	case IN:
		if ( use_servers || asis_servers || ign_servers ||
		     use_types || asis_types || ign_types || equiv==0 ||
		     ddi_list.first || ddo_list.first )
			syntax() ;
		tail_archive() ;
		break ;
	case OUT:
		if ( force || retain || dir_servers || file_servers )
			syntax() ;
		if ( ddi_list.first ) act=RESCAN ;
		chk_st();
		tail_archive() ;
		break ;
	case LIST:
		if ( use_servers || asis_servers || ign_servers ||
		     use_types || asis_types || ign_types || equiv==0  ||
		     ddi_list.first || ddo_list.first )
			syntax() ;
		if ( force || retain ) syntax() ;
		tail_archive() ;
		break ;
	case NAMES:
		chk_st();
		if ( force || retain || io_type!=IO_FILE ||
		     sz_buf || sz_medium || arch_list.first ||
		     ddi_list.first || ddo_list.first ) syntax() ;
		break ;
	case PRIV:
		if ( dir_servers || file_servers ) syntax() ;
		if ( retain || equiv==0 ) syntax() ;
		if ( io_type!=IO_FILE || sz_buf || sz_medium || arch_list.first ||
		     ddi_list.first || ddo_list.first ) syntax() ;
		chk_st();
		break ;
	}
	chk_sz(sz_buf) ;
	{	/* Get the file name arguments */
		register struct pathlist *new, *old ;
		int	pathlength ;
		old=files;
		for (; optind < argc; optind++) {
			new=MYMEM(struct pathlist);
			new->next= (struct pathlist *)0 ;
			if ( !(act==IN || act==LIST) ) {
				/* allocate space for new path */
				pathlength=strlen(argv[optind]);
				new->path= (char *)getmem(pathlength+1);
				/* normalize it */
				if ( path_norm(NULL,argv[optind],new->path,
						pathlength) == -1 ) {
					fatal("could not normalize \"%s\"",
						argv[optind]);
				}
				if ( *new->path==0 ) {
					strcpy(new->path,".");
				}
			} else {
				new->path=argv[optind] ;
			}
			if ( !old ) {
				files=new ;
			} else {
				old->next=new ;
			}
			old=new ;
		}
	}
}

void
chk_st()
{
	/* Check servertype arguments for consistency */

	if ( asis_types && ign_types ) {
		fatal("only one of -y and -z may be specified") ;
	}
	if ( !use_types ) use_types= USE_DEFAULT ; /* The default */
	if ( asis_types && strpbrk(use_types,asis_types) ) {
		fatal("-y argument (%s) overlaps -u argument (%s)",
			asis_types,use_types) ;
	}
	if ( ign_types && strpbrk(use_types,ign_types) ) {
		fatal("-z argument (%s) overlaps -u argument (%s)",
			ign_types,use_types) ;
	}
	def_method= (asis_types||asis_servers) ? M_IGNORE : M_CAP ;
}

unsigned long medm_sz(s)
	char	*s;
{
	/* convert N to number, suffixes:
		b 	=>	*512
		k	=>	*1024
		m	=>	*1024*1024
		g	=>	*1024*1024*1024
	*/
	unsigned long			sz ;
	unsigned long			szp ;
	long				mult ;
	char				ind ;
	char				nochar ;
	int				nconv ;

	nconv=sscanf(s,"%ld%c%1s",&sz,&ind,&nochar);
	if ( nconv==1 ) return sz ;
	if ( nconv<1 || nconv>2) {
		fprintf(stderr,"illegal number \"%s\" in parameter\n",s) ;
		syntax() ;
	}
	mult=1 ;
	switch( ind ) {
	case 'g':
	case 'G':
		mult*=1024 ;
	case 'm':
	case 'M':
		mult*=1024 ;
	case 'k':
	case 'K':
		mult*=2 ;
	case 'b':
	case 'B':
		mult*= 512 ;
		if ( mult!=0 ) {/* Dirty, overflow results in zero */
			szp=sz ; sz*=mult ;
			if ( sz/mult==szp ) break ;
		}
	default:
		fprintf(stderr,"illegal number \"%s\" in parameter\n",s) ;
		syntax() ;
	}
	return sz ;
}	

void		
initargs()
{
	register struct	pathlist	*flist ;
	errstat				errcode;


	if ( ( act==OUT || act==RESCAN || act==NAMES || act==PRIV ) ) {
		for ( flist=xlist ; flist ; flist=flist->next ) {
			errcode=sp_lookup(SP_DEFAULT,flist->path,
							&flist->caps) ;
			if ( errcode!=STD_OK )
				fatal("can not open %s",flist->path) ;
		}
		for ( flist=files ; flist ; flist=flist->next ) {
			errcode=sp_lookup(SP_DEFAULT,flist->path,
							&flist->caps) ;
			if ( errcode!=STD_OK )
				fatal("can not open %s",flist->path) ;
		}
	}
	ini_slist(priv_servers) ;
	ini_slist(dir_servers) ;
	ini_slist(file_servers) ;
	ini_slist(use_servers) ;
	ini_slist(ign_servers) ;
	ini_slist(asis_servers) ;
	f_cfd(ign_servers,asis_servers,"ignore","copy");
	f_cfd(ign_servers,use_servers,"ignore","use");
	f_cfd(asis_servers,use_servers,"copy","use");
}

void
f_cfd(list1,list2,s1,s2)
	struct	serverlist	*list1,*list2 ;
	char			*s1,*s2;
{
	register struct	serverlist	*slist1=list1 ;
	register struct	serverlist	*slist2=list2 ;

	for ( slist1=list1 ; slist1 ; slist1=slist1->next ) {
		for ( slist2=list2 ; slist2 ; slist2=slist2->next ) {
			if ( SAME_SERVER(&slist1->server,&slist2->server) ) {
				fatal("cannot %s and %s from server (%s,%s)",
s1,s2,slist1->path,slist2->path) ;
			}
		}
	}
}

void
ini_slist(list)
	struct	serverlist	*list ;
{
	register struct	serverlist	*slist=list ;
	int				len ;
	capset				dir;
	capset				contents;
	errstat				errcode;

	/* Two kinds of servers are distinguished: directory servers
	   and others
	 */
	for ( slist=list ; slist ; slist=slist->next ) {
		errcode=name_lookup(slist->path,&slist->server) ;
		if ( errcode!=STD_OK ) fatal("can not open %s",slist->path) ;
		slist->i_type=0 ;
		errcode=std_info(&slist->server,&slist->i_type,1,&len);
		slist->type=C_OTHER ;
		if ( !( errcode==STD_OK || errcode==STD_OVERFLOW) ) {
			warning("can not determine type of %s",slist->path) ;
		} else {
			cs_singleton(&dir,&slist->server) ;
			if ( sp_lookup(&dir,"",&contents)==STD_OK ) {
				slist->type=C_DIR ;
				cs_free(&contents);
			}
			cs_free(&dir);
		}
		if ( debug >0 ) {
			printf("Server %s has type %c and is%s a directory server\n",
				slist->path, slist->i_type,
				( slist->type==C_DIR? "" : " not") ) ;
		}
	}
}

/* Multiple archive handling ... */

int
#ifdef __STDC__
f_next(struct f_head_t *fl)
#else
f_next(fl)
	struct f_head_t		*fl;
#endif
{
	struct f_list_t		*cur_in ;

	if ( !fl->next ) return 0 ;
	cur_in= fl->next ;
	fl->next= cur_in->next ;
	io_type=cur_in->type ;
	archive= cur_in->path ;
	return 1 ;
}

/* Setup read for multiple archives ... */
void
#ifdef __STDC__
f_insert(struct f_head_t *fl,char *path,int type)
#else
f_insert(fl,path,type)
	struct f_head_t		*fl;
	char			*path;
	int			type;
#endif
{
	struct f_list_t		*newdef ;

	newdef= (struct f_list_t *)getmem(sizeof(struct f_list_t));
	newdef->type=type ;
	newdef->path=keepstr(path);
	newdef->next= (struct f_list_t *)0;
	if ( fl->last ) {
		fl->last->next=newdef ;
	} else {
		fl->first=newdef ;
		fl->next=newdef ;
	}
	fl->last=newdef ;
}


void tail_archive() {
	extern char *getenv();

	if ( io_type==IO_TAPE || io_type==IO_TAPE_NR ) {
		archive=getenv("TAPE");
		if ( !archive ) {
			fatal("no name specified for archive tape") ;
		}
		f_insert(&arch_list,archive,io_type);
		io_type=IO_FILE ;
	} else if ( io_type==IO_FLOP ) {
		archive=getenv("FLOPPY");
		if ( !archive ) {
			fatal("no name specified for archive floppy") ;
		}
		f_insert(&arch_list,archive,io_type);
		io_type=IO_FILE ;
	}
	if ( !arch_list.first ) {
		/* No tape or file usage, use stdin/stdout */
		f_insert(&arch_list,"-",IO_FILE);
	}
	if ( !f_next(&arch_list) ) fatal("internal: first arch");
	/* if More the one -f flag */
	if ( arch_list.next ) {
		if ( !(act==IN || act==LIST)) {
			fatal("multiple -f only with -i and -t");
		}
		in_multi = 1 ;
	}
}

void syntax() {
	fprintf(stderr,"syntax:\n");
	fprintf(stderr,"%s -o [-[mMl] -f file] [-v] [-s N[bkmg]] [-b N[bkmg]]\n\
\t[-[mMl] -a file] [-[mMl] -A file] [-n N] [-S]\n\
\t[-x path] [-X pattern] [-u types] [[-y types] [-z types]]\n\
\t[-U server-path ] [-Y server-path] [-Z server-path ]\n\
\tname ...\n",progname);
	fprintf(stderr,"%s -i [-[mMl] -f file] ...  [-[rw]] [-v] [-s N[bkmg]]\n\
\t[-b N[bkmg]] [-n N] [-S]\n\
\t[-x path] [-X pattern] [-k path]\n\
\t[-B server-path ] [-D server-path ] [name ...]\n",progname);
	fprintf(stderr,"%s -t [-[mMl] -f file] ... [-v] [-s N[bkmg]]\n\
\t[-b N[bkmg]] [-n N] [-S]\n\
\t[-x path] [-X pattern] [name ...]\n",
			progname);
#ifdef NOTDEF
	fprintf(stderr,"%s -c [-v] [-[rf]] [-x path] [-X pattern]\n\
\t[-u types] [[-y types] [-z types]]\n\
\t[-U server-path ] [-Y server-path] [-Z server-path]\n\
\t[-B server-path ] [-D server-path ] src dest\n",progname);
#endif
	fprintf(stderr,"%s -g [-D server-path ] [-x path] [-X pattern]\n\
\t[-u types] [[-y types] [-z types]]\n\
\t[-U server-path ] [-Y server-path] [-Z server-path ]\n\
\tname ...\n",progname);
	exit(9);
}

/* Routines to check whether a given entry is interesting */
/* Only usefull for existing files (-o, -c, -g) */

int
checkserv(cap)
	capability			*cap ;
{
	register struct	pathlist	*flist ;
	register struct serverlist	*slist ;

	/* First check whether we should ignore the entry */
	/* Check -x */
	for ( flist=xlist ; flist ; flist=flist->next ) {
		if ( cap_in_cs(cap,&flist->caps) ) {
			return M_IGNORE ; /* Not to be considered */
		}
	}
	/* Check -Z */
	for ( slist=ign_servers ; slist ; slist=slist->next ) {
		if ( slist->type!=C_DIR ) continue ;
		if(SAME_SERVER(&slist->server,cap)) return M_IGNORE ;
	}

	/* Then check whether we should use the entry */
	/* Check -Y */
	for ( slist=asis_servers ; slist ; slist=slist->next ) {
		if ( slist->type!=C_DIR ) continue ;
		if(SAME_SERVER(&slist->server,cap)) return M_CAP ;
	}
	/* Check -U */
	for ( slist=use_servers ; slist ; slist=slist->next ) {
		if ( slist->type!=C_DIR ) continue ;
		if(SAME_SERVER(&slist->server,cap)) return M_CONTENT ;
	}

	/* We now have to fall back on type information */
	/* First check whether there is any; if not, avoid to call
	   std_info
	 */
	if ( ( !ign_types || ign_types[0]==0 ) &&
	     ( !asis_types || asis_types[0]==0 ) &&
	     ( !use_types || use_types[0]==0 ) ) {
		return def_method ;
	}
	return M_UNKNOWN ; /* zero */
}

int
testcs(cs)
	capset			*cs ;
{
	register struct	pathlist	*flist ;
	register struct serverlist	*slist ;
	/* only static to avoid cost of dynamic allocation */
	static char			info[100] ;
	int				len ;
	errstat				errcode ;

	/* First check whether we should ignore the entry */
	/* Check -x */
	for ( flist=xlist ; flist ; flist=flist->next ) {
		if ( cs_overlap(cs,&flist->caps) ) {
			return M_IGNORE ; /* Not to be considered */
		}
	}
	/* Check -Z */
	for ( slist=ign_servers ; slist ; slist=slist->next ) {
		if ( slist->type!=C_DIR ) continue ;
		if(port_in_cs(&slist->server,cs)) return M_IGNORE ;
	}

	/* Then check whether we should use the entry */
	/* Check -Y */
	for ( slist=asis_servers ; slist ; slist=slist->next ) {
		if ( slist->type!=C_DIR ) continue ;
		if(port_in_cs(&slist->server,cs)) return M_CAP ;
	}
	/* Check -U */
	for ( slist=use_servers ; slist ; slist=slist->next ) {
		if ( slist->type!=C_DIR ) continue ;
		if(port_in_cs(&slist->server,cs)) return M_CONTENT ;
	}

	/* We now have to fall back on type information */
	/* First check whether there is any, if not, avoid to call
	   cs_info
	 */
	if ( ( !ign_types || ign_types[0]==0 ) &&
	     ( !asis_types || asis_types[0]==0 ) &&
	     ( !use_types || use_types[0]==0 ) ) {
		return def_method ;
	}
	errcode=cs_info(cs,info,sizeof info,&len) ;
	if ( !( errcode==STD_OK || errcode==STD_OVERFLOW) ) {
		return def_method ;
	}
	/* Check -z */
	if ( ign_types && ign_types[0]!=0 && strchr(ign_types,info[0]) )
		return M_IGNORE ;
	/* Check -y */
	if ( asis_types && asis_types[0]!=0 && strchr(asis_types,info[0]) )
		return M_CAP ;
	/* Check -u (has default) */
	if ( use_types && use_types[0]!=0 && strchr(use_types,info[0]) )
		return M_CONTENT ;
	/* Ok, does not seem to fit, return the default */
	return def_method ;
}

/* A version for a single capability */
int
testcap(cap)
	capability			*cap ;
{
	/* only static to avoid cost of dynamic allocation */
	static char			info[100] ;
	int				len ;
	errstat				errcode ;
	int				retval ;

	/* First check -U etc. */
	retval= checkserv(cap) ;
	if ( retval!=M_UNKNOWN ) return retval ;

	errcode=std_info(cap,info,sizeof info,&len) ;
	if ( !( errcode==STD_OK || errcode==STD_OVERFLOW) ) {
		return def_method ;
	}
	/* Check -z */
	if ( ign_types && ign_types[0]!=0 && strchr(ign_types,info[0]) )
		return M_IGNORE ;
	/* Check -y */
	if ( asis_types && asis_types[0]!=0 && strchr(asis_types,info[0]) )
		return M_CAP ;
	/* Check -u (has default) */
	if ( use_types && use_types[0]!=0 && strchr(use_types,info[0]) )
		return M_CONTENT ;
	/* Ok, does not seem to fit, return the default */
	return def_method ;
}

int testfile(cs,entry)
	capset			*cs ;
	char			*entry ;
{
	register struct patternlist	*plist ;
	int				i;
	int				res;

	/* Check -X */
	for ( plist=xxlist ; plist ; plist=plist->next ) {
		if (fnmatch(plist->pattern,entry,0)) return M_IGNORE ;
	}
	if ( equiv ) return testcs(cs) ;
	res=0 ;
	for ( i = 0; i<cs->cs_final; i++ ) {
		if ( cs->cs_suite[i].s_current ) {
			res|=testcap(&cs->cs_suite[i].s_object) ;
		}
	}
	/* Empty set, use default action */
	return res?res:def_method ;
}

/* Special test executed when revisting a directory graph */
int
rv_test(pathp)
	dgw_paths               *pathp;
{
	long				id;

	if ( !pathp->dotdot ) return 0 ; /* True for entrance(changed dir) */

	id= obj_exists(&pathp->cap) ;

	if ( id==0 ) return 0 ; /* New, not seen before */

	/* We have seeen it before, if it is not a directory we do
	 * not bother to look at it any further, because we assume that it
	 * is an immutable object (bullet file?).
	 * If it is an existing directory we will come to it later, revisit
	 * goes through all existing dir's.
	 */
	return 1 ;
}

	
/* Walk the tree -i, -g and -c */
int
testdir(pathp)
	dgw_paths		*pathp;
{
	register struct	pathlist	*flist ;
	register struct patternlist	*plist ;
	register struct serverlist	*slist ;
	/* only static to avoid cost of dynamic allocation */
	static char			info[100] ;
	int				len ;
	errstat				errcode ;
	capset				dircs ;
	SP_DIR				*dd ;

	/* When rescanning stop at dir's we have seen */
	if ( act==RESCAN && rv_test(pathp) ) return DGW_STOP ;

	/* Check -x */
	for ( flist=xlist ; flist ; flist=flist->next ) {
		if ( cap_in_cs(&pathp->cap,&flist->caps) ) {
			return DGW_STOP ; /* Not to be in the graph */
		}
	}
	/* Check -X */
	for ( plist=xxlist ; plist ; plist=plist->next ) {
		if (fnmatch(plist->pattern,pathp->entry,0)) return DGW_STOP ;
	}
	/* Check -Z */
	for ( slist=ign_servers ; slist ; slist=slist->next ) {
		if ( slist->type!=C_DIR ) continue ;
		if(SAME_SERVER(&pathp->cap,&slist->server)) return DGW_STOP ;
	}
	/* Check -Y */
	for ( slist=asis_servers ; slist ; slist=slist->next ) {
		if ( slist->type!=C_DIR ) continue ;
		if(SAME_SERVER(&pathp->cap,&slist->server)) return DGW_STOP ;
	}

	if ( !pathp->dotdot ) {
		/* top level entries might not be dirs */
		if ( !cs_singleton(&dircs,&pathp->cap) ) {
			fatal("cs_singleton failed in testdir");
		}
		errcode=sp_list(&dircs,&dd);
		cs_free(&dircs); 
		if ( errcode!=STD_OK ) return DGW_STOP ;
		sp_closedir(dd);
	}

	/* Check whether it belongs to a server */
	if ( use_types && use_types[0]==0 ) {
		/* Already tested by the dirgraph module */
		return DGW_OK ;
	}
	/* Check -U */
	for ( slist=use_servers ; slist ; slist=slist->next ) {
		if ( slist->type!=C_DIR ) continue ;
		if(SAME_SERVER(&pathp->cap,&slist->server)) return DGW_OK ;
	}
	/* Check -u, which has a default */
	if ( use_types ) {
		errcode=std_info(&pathp->cap,info,sizeof info,&len) ;
		if ( !( errcode==STD_OK || errcode==STD_OVERFLOW) ) {
			return DGW_AGAIN ;
		}
		if ( strchr(use_types,info[0]) ) return DGW_OK ;
	}
	return DGW_STOP ;
}

int
dowalk(func)
        int                     (*func)();
{
	capset			**servers ;
	int			retval ;

	servers=srch_servers();
	retval=duck(func,servers,files) ;
	fr_servers(servers) ;
	return retval ;
}

int
duck(func,servers,wfiles)
	int			(*func)();
	capset			**servers;
	struct pathlist		*wfiles ;
{
        dgw_params	dgwp;
        dgw_paths	*current;
        dgw_paths	*last;
        errstat		err;
	register struct pathlist *flist ;

        dgwp.mode=DGW_ALL;
        dgwp.dodir=func;
        dgwp.testdir=testdir;
        dgwp.doagain=NULL;
        dgwp.servers=servers;

	/* Fetch entries into tree */
        dgwp.entry=last= (dgw_paths *)0;
	for ( flist=wfiles ; flist ; flist=flist->next ) {
                current=MYMEM(dgw_paths);
                err=cs_to_cap(&flist->caps,&current->cap);
                if ( err!=STD_OK ) {
			warning("missing capability, can not rescan %s",
				flist->path);
			free(current);
			continue ;
		}
                current->entry=flist->path;
                current->next= (dgw_paths *)0;
                if ( last ) last->next=current ; else dgwp.entry=current ;
                last=current;
                /* First `find' the top level paths */
                /* func(current->entry,&flist->caps,(capset *)NULL,""); */
        }
	if ( !dgwp.entry ) {
		/* All capabilities went missing */
		return 0 ;
	}
        err=dgwalk(&dgwp);
	/* Action performed, free memory used for call */
	last=dgwp.entry ;
	while ( last ) {
		current=last;
		last=last->next ;
		free((char *)current);
	}
	return err==STD_OK ; /* OK */
}

capset **
srch_servers()
{
	register struct serverlist *slist ;
	capset		**retval ;
	int		n_servers, max_servers ;
	capset		*cs;

	retval= (capset **)NULL ;
	n_servers=max_servers=0 ; /* get directory servers */
	if ( !use_types || use_types[0]==0 ) {
		max_servers=10;
		retval=(capset **)
			getmem((unsigned)(max_servers*sizeof(capset *)));
		for ( slist=use_servers ; slist ; slist=slist->next ) {
			if ( slist->type!=C_DIR ) continue ;
	                cs=MYMEM(capset);
			if ( !cs_singleton(cs,&slist->server) ) fatal("Out of mem");
			n_servers++ ;
			if ( n_servers>max_servers ) {
				max_servers+=10;
				retval= (capset **) adjmem((ptr)retval,
				   (unsigned)(max_servers*sizeof(capset *)));
			}
			retval[n_servers]=(capset *)0 ;
			retval[n_servers-1]= cs ;
		}
	}			
	return retval ;
}

void
fr_servers(list)
	capset			**list ;
{
	capset			**p_cs ;

	if ( list ) {
		for ( p_cs=list ; *p_cs ; p_cs++ ) {
			cs_free(*p_cs) ;
			free((char *)*p_cs) ;
		}
		free((char *)list);
	}
}


/* Comparison of capabilities */
int
cap_eq(a, b)
	register capability *a, *b;
{
	register private *p, *q;
	rights_bits r;
	
	if (!PORTCMP(&a->cap_port, &b->cap_port))
		return 0;
	p = &a->cap_priv;
	q = &b->cap_priv;
	if (prv_number(p) != prv_number(q))
		return 0;
	if (p->prv_rights == q->prv_rights)
		return PORTCMP(&p->prv_random, &q->prv_random);
	if (p->prv_rights == ALL_RIGHTS)
		return prv_decode(q, &r, &p->prv_random) == 0;
	if (q->prv_rights == ALL_RIGHTS)
		return prv_decode(p, &r, &q->prv_random) == 0;
	/* Incomparable -- assume they are equal */
	return 2;
}

int cap_in_cs(cap,cs)
	capability		*cap;
	capset			*cs;
{
	int		i;


	for ( i = 0; i<cs->cs_final; i++ ) {
		if ( cs->cs_suite[i].s_current ) {
			if ( cap_eq(cap,&cs->cs_suite[i].s_object)) return 1;
		}
	}
	return 0 ;
}

int cs_overlap(cs1,cs2)
	capset			*cs1,*cs2;
{
	int		i,j;


	for ( i = 0; i<cs1->cs_final; i++ ) {
		if ( cs1->cs_suite[i].s_current ) {
			for ( j = 0; j<cs2->cs_final; j++ ) {
				if ( cs2->cs_suite[j].s_current ) {
if ( cap_eq(&cs1->cs_suite[i].s_object,&cs2->cs_suite[j].s_object))return 1 ;
				}
			}
		}
	}
	return 0 ;
}

int port_in_cs(cap,cs)
	capability		*cap;
	capset			*cs;
{
	register suite		*a;
	register int		i;
	
	for (i = cs->cs_final, a = cs->cs_suite; --i >= 0; ++a) {
		if (!a->s_current)
			continue;
		if ( SAME_SERVER(&a->s_object,cap )) {
			return 1;
		}
	}
	return 0;
}

/* Like cs_free, reset to initial state and free only of not using original
   suite.
 */
void
cs_reset(cs)
capset *cs;
{
 
    register int	i ;

    
    /* The condition below happens when cs is part of cap_t */
    if ( (suite *)(cs+1) != cs->cs_suite ) {
	/* Suite * was malloced, free it */
	free(cs->cs_suite);
	cs->cs_suite= (suite *)(cs+1) ;
	cs->cs_final= CAPS_IN_ID ;
    }

    for ( i=0 ; i<cs->cs_final ; i++ ) cs->cs_suite[i].s_current=0 ;
    cs->cs_initial= 0 ;
}

/* Like cs_copy, but try to use existing suite in new and leave
   capabilities already there intact
 */
void
cs_transfer(recv, cs)
capset *recv, *cs;
{
 
    register int	in_cur ;
    register int	recv_cur ;
    int			in_lookahead ;
    register suite	*new_suite ;
    unsigned int	cnt_slots ;
 
    for ( recv_cur=0, in_cur=INI_CS(cs) ; in_cur<cs->cs_final ; in_cur++) {
	if ( cs->cs_suite[in_cur].s_current ) {
	    /* Valid entry */
	    /* First search empty slot */
	    for (;;) {
		if ( recv_cur==recv->cs_final ) {
		    /* Not enough slots, create more */
		    cnt_slots=recv->cs_final+1 ;
		    for ( in_lookahead= in_cur+1 ; in_lookahead<cs->cs_final ;
			  in_lookahead++ ) {
			if ( cs->cs_suite[in_lookahead].s_current ) {
			    cnt_slots++ ;
			}
		    }
		    new_suite=(suite *) calloc(sizeof(suite),cnt_slots) ;
		    if ( !new_suite ) fatal("Not enough memory in cs_transfer");
		    memcpy((void *)new_suite, recv->cs_suite,
				sizeof(suite)*recv->cs_final) ;
		    if ( (suite *)(recv+1) != recv->cs_suite ) {
			/* Suite * was malloced, free it */
			free(recv->cs_suite);
		    }
		    recv->cs_suite= new_suite ;
		    recv->cs_final= cnt_slots ;
		    /* Leave recv_cur at previous recv->cs_final */
		    break;
		}
		/* Finally, we can copy */
		if ( recv->cs_suite[recv_cur].s_current==0 ) break ;
		recv_cur++ ;
	    }
	    recv->cs_suite[recv_cur] = cs->cs_suite[in_cur];
	    if ( recv_cur<recv->cs_initial ) recv->cs_initial=recv_cur ;
	}
    }
}

/* Like cs_singleton, but try to use existing suite in new and leave
   capabilities already there intact
 */
void
cs_insert(recv, cap)
capset *recv;
capability *cap;
{
 
    register int	recv_cur ;
    register suite	*new_suite ;
    unsigned int	cnt_slots ;
    int			in_lookahead ;
 
    for (recv_cur=0;;) {
	if ( recv_cur==recv->cs_final ) {
	    /* Not enough slots, create more */
	    cnt_slots=recv->cs_final+1 ;
	    for ( in_lookahead= recv_cur+1 ; in_lookahead<recv->cs_final ;
		  in_lookahead++ ) {
		if ( recv->cs_suite[in_lookahead].s_current ) {
		    cnt_slots++ ;
		}
	    }
	    new_suite=(suite *) calloc(sizeof(suite),cnt_slots) ;
	    if ( !new_suite ) fatal("Not enough memory in cs_insert");
	    memcpy((void *)new_suite, recv->cs_suite,
			sizeof(suite)*recv->cs_final) ;
	    if ( (suite *)(recv+1) != recv->cs_suite ) {
		/* Suite * was malloced, free it */
		free(recv->cs_suite);
	    }
	    recv->cs_suite= new_suite ;
	    recv->cs_final= cnt_slots ;
	    /* Leave recv_cur at previous recv->cs_final */
	    break;
	}
	if ( recv->cs_suite[recv_cur].s_current==0 ) break ;
	recv_cur++ ;
    }
    /* Finally, we can copy */
    recv->cs_suite[recv_cur].s_object= *cap ;
    recv->cs_suite[recv_cur].s_current= 1 ;
    if ( recv_cur<recv->cs_initial ) recv->cs_initial=recv_cur ;
}

errstat
#ifdef __STDC__
cs_info(capset *cs, char *s, int in_len, int *out_len)
#else
cs_info(cs,s,in_len,out_len)
	capset		*cs;
	char		*s;
	int		in_len;
	int		*out_len ;
#endif
{
	int		done_one ;
	int		can_reset ;
	errstat		errcode;
	register int	i ;

	can_reset=1 ; done_one=0 ;
retry:
	for ( i = 0; i<cs->cs_final; i++ ) {
		if ( cs->cs_suite[i].s_current ) {
			done_one=1 ;
			errcode=gp_std_info(&cs->cs_suite[i].s_object,s,in_len,out_len) ;
			if ( errcode==RPC_NOTFOUND ) continue ;
			return errcode ;
		}
	}
	if ( done_one ) {
		if ( can_reset ) {
			gp_badport((port *)0,GP_INIT) ;
			can_reset=0 ;
			goto retry ;
		}
		return errcode ;
	} else {
		return RPC_NOTFOUND ;
	}
}

errstat
#ifdef __STDC__
cs_b_size(capset *cs, b_fsize *size)
#else
cs_b_size(cs,size)
	capset		*cs;
	b_fsize		*size ;
#endif
{
	int		can_reset ;
	int		done_one ;
	errstat		errcode;
	register int	i ;

	can_reset=1 ; done_one=0 ;
retry:
	for ( i = 0; i<cs->cs_final; i++ ) {
		if ( cs->cs_suite[i].s_current ) {
			done_one=1 ;
			if ( gp_badport(&cs->cs_suite[i].s_object.cap_port,GP_LOOKUP) ) {
				continue ;
			}
			switch ( checkserv(&cs->cs_suite[i].s_object) ) {
			case M_UNKNOWN :
			case M_CONTENT :
				break ;
			default :
				continue ;
			}
			errcode=b_size(&cs->cs_suite[i].s_object,size) ;
			if ( errcode==RPC_NOTFOUND ) continue ;
			if ( errcode==STD_OK ) return STD_OK ;
		}
	}
	if ( done_one ) {
		if ( can_reset ) {
			gp_badport((port *)0,GP_INIT) ;
			can_reset=0 ;
			goto retry ;
		}
		return errcode ;
	} else {
		return RPC_NOTFOUND ;
	}
}

errstat
#ifdef __STDC__
cs_b_read(capset *cs, b_fsize offset, char *buf,
	  b_fsize to_read, b_fsize *were_read)
#else
cs_b_read(cs,offset,buf,to_read,were_read)
	capset		*cs;
	b_fsize		offset;
	char		*buf;
	b_fsize		to_read;
	b_fsize		*were_read;
#endif
{
	int		can_reset ;
	int		done_one ;
	errstat		errcode;
	register int	i ;

	can_reset=1 ; done_one=0 ;
retry:
	for ( i = 0; i<cs->cs_final; i++ ) {
		if ( cs->cs_suite[i].s_current ) {
			done_one=1 ;
			if ( gp_badport(&cs->cs_suite[i].s_object.cap_port,GP_LOOKUP) ) {
				continue ;
			}
			switch ( checkserv(&cs->cs_suite[i].s_object) ) {
			case M_UNKNOWN :
			case M_CONTENT :
				break ;
			default :
				continue ;
			}
			errcode=b_read(&cs->cs_suite[i].s_object,
				offset,buf,to_read,were_read) ;
			if ( errcode==RPC_NOTFOUND ) continue ;
			if ( errcode==STD_OK ) return STD_OK ;
		}
	}
	if ( done_one ) {
		if ( can_reset ) {
			gp_badport((port *)0,GP_INIT) ;
			can_reset=0 ;
			goto retry ;
		}
		if ( errcode==STD_NOMEM ) {
			warning("Bullet server is out of memory") ;
			sleep(350);
			goto retry ;
		}
		return errcode ;
	} else {
		return RPC_NOTFOUND ;
	}
}

/* Memory allocation */

char *keepstr(s)
	char *s ;
{
	char *k ;

	k=(char *)getmem(strlen(s)+1) ;
	strcpy(k,s) ;
	return k ;
}

ptr keepmem(s,n)
	char		*s ;
	unsigned	n ;
{
	ptr   k ;

	k=getmem(n) ;
	memcpy((char *)k,s,n) ;
	return (ptr)k ;
}

#ifdef MEMDEBUG

void
initmem() {}

#else

void
initmem() {
	unsigned	trysize ;

	trysize=(MEM_EXTRA+1023)/1024 ;
	do {
		extramem=(ptr)calloc(trysize*1024,1) ;
		if ( extramem ) break ;
		trysize= trysize/2;
	} while ( trysize ) ;
}

#ifdef __STDC
ptr getmem(size_t size)
#else
ptr getmem(size)
	unsigned size ;
#endif
{
	ptr		newmem;

	/* Try to allocate what you need */
	if ( size==0 ) fatal("internal: allocating zero sized block") ;
	newmem=(ptr)calloc(size,1) ;
	/* If no mem or not enough left try to cleanup */
	if ( !newmem ) {
		if ( recursive ) fatal("out of memory") ;
		recursive=1 ;
		findmem() ; /* Might release some memory */
		recursive=0 ;
		newmem=(ptr)calloc(size,1) ;
		if ( !newmem )  fatal("out of memory") ;
		initmem() ;
	}
	return newmem ;
}
#endif

#ifdef __STDC
ptr adjmem(ptr mem,size_t size)
#else
ptr adjmem(mem,size)
	ptr	 mem ;
	unsigned size ;
#endif
{
	ptr		newmem;


	/* Try to allocate what you need */
	newmem=(ptr)realloc((char *)mem,size) ;
	/* If no mem or not enough left try to cleanup */
	if ( !newmem ) {
		if ( recursive ) fatal("out of memory in realloc") ;
		recursive=1 ;
		findmem() ; /* Might release some memory */
		recursive=0 ;
		newmem=(ptr)realloc((char *)mem,size) ;
		if ( !newmem )  fatal("out of memory in realloc") ;
		initmem() ;
	}
	return newmem ;
}

void
findmem() { /* Empty for the moment */ }

/* Error reporting etc. */

void
soap_err(err)
	errstat		err ;
{
	fatal("receiving error \"%s\" from directory server",err_why(err)) ;
}

void
bullet_err(err)
	errstat		err ;
{
	warning("receiving error \"%s\" from bullet server",err_why(err)) ;
}

/*VARARGS*/
#ifdef __STDC__
void fatal(char *fmt, ...)
{
	va_list		args;
#ifdef MULTI_THREADED
	int		lock_success ;
#endif

	va_start(args,fmt);
#else
void fatal(va_alist) va_dcl
{
	char	*fmt;
	va_list		args;
#ifdef MULTI_THREADED
	int		lock_success ;
#endif

	va_start(args);
	fmt= va_arg(args, char *);
#endif
#ifdef MULTI_THREADED
	/* In fatal we do not wish to wait forever */
	lock_success= mu_trylock(&lck_errfile,2000);
#endif
	fprintf(errfile,"%s: ",progname);
	vfprintf(errfile,fmt,args);
	putc('\n',errfile);
#ifdef MULTI_THREADED
	if ( lock_success==0 ) mu_unlock(&lck_errfile);
#endif
	va_end(args);
	if ( act==IN ) del_unreach() ;
	exit(1);
}

/*VARARGS*/
/* The same message as fatal, but no exit. This is called from threads.
   It is the responsibility of the thread to terminate.
   This should be done by calling t_finish.
 */
#ifdef __STDC__
void t_fatal(int mt_flag,char *fmt, ...)
{
	va_list		args;
#ifdef MULTI_THREADED
	int		lock_success ;
#endif

	va_start(args,fmt);
#else
void t_fatal(va_alist) va_dcl
{
	char		*fmt;
	int		mt_flag ;
	va_list		args;
#ifdef MULTI_THREADED
	int		lock_success ;
#endif

	va_start(args);
	mt_flag= va_arg(args, int);
	fmt= va_arg(args, char *);
#endif
#ifdef MULTI_THREADED
	/* In fatal we do not wish to wait forever */
	lock_success= mu_trylock(&lck_errfile,2000);
#endif
	fprintf(errfile,"%s: ",progname);
	vfprintf(errfile,fmt,args);
	putc('\n',errfile);
#ifdef MULTI_THREADED
	if ( lock_success==0 ) mu_unlock(&lck_errfile);
#endif
	va_end(args);
	if ( !mt_flag ) t_finish() ;
}

void t_finish() {
	if ( act==IN ) del_unreach() ;
	if ( arch_valid ) {
		if ( act==OUT ) std_destroy(&arch_cap);
	}
	if ( io_type==IO_TAPE ) tape_rewind(&arch_cap,1) ;
	exit(1);
}

/* We could have avoided this routine if there were macros with a variable
 * number of args.
 */

#ifdef __STDC__
void warning(char *fmt, ...)
#else
void warning(va_alist) va_dcl
#endif
{
	va_list		args;
#ifdef __STDC__
	va_start(args,fmt);
#else
	char		*fmt;

	va_start(args);
	fmt= va_arg(args, char *);
#endif
#ifdef MULTI_THREADED
	mu_lock(&lck_errfile);
#endif
	fprintf(errfile,"%s: warning, ",progname);
	vfprintf(errfile,fmt,args);
	putc('\n',errfile);
#ifdef MULTI_THREADED
	mu_unlock(&lck_errfile);
#endif
	va_end(args);
}

/*VARARGS*/
#ifdef __STDC__
void message(char *fmt, ...)
{
	va_list		args;

	va_start(args,fmt);
#else
void message(va_alist) va_dcl
{
	char		*fmt;
	va_list		args;

	va_start(args);
	fmt= va_arg(args, char *);
#endif
#ifdef MULTI_THREADED
	mu_lock(&lck_errfile);
#endif
	vfprintf(errfile,fmt,args);
	putc('\n',errfile);
#ifdef MULTI_THREADED
	mu_unlock(&lck_errfile);
#endif
	va_end(args);
}

#ifdef __STDC__
void err_flush(void)
#else
void err_flush()
#endif
{
#ifdef MULTI_THREADED
	mu_lock(&lck_errfile);
#endif
	fflush(errfile);
#ifdef MULTI_THREADED
	mu_unlock(&lck_errfile);
#endif
}

#ifdef MULTI_THREADED
/* Code to use the locking from outside this module */

void
#ifdef __STDC__
err_lock(void)
#else
err_lock()
#endif
{
	mu_lock(&lck_errfile);
}

void
#ifdef __STDC__
err_unlock(void)
#else
err_unlock()
#endif
{
	mu_unlock(&lck_errfile);
}

#endif /* MULTI_THREADED */

#ifdef __STDC__
void ini_talk(void)
#else
void ini_talk()
#endif
{
	FILE		*tryopen ;
	static int	done ;

	if ( done ) return ;
	done=1 ;
        tryopen=fopen("/dev/tty","r") ;
        if ( tryopen!=NULL ) {
                i_talkfile= tryopen ;
        } else {
                fprintf(stderr,"could not open /dev/tty\n");
        }
        tryopen=fopen("/dev/tty","w") ;
        if ( tryopen!=NULL ) {
                o_talkfile= tryopen ;
        } else {
                fprintf(stderr,"could not open /dev/tty\n");
        }
        setvbuf(o_talkfile,(char *)0,_IONBF,(size_t) 0);
}


int
#ifdef __STDC__
answer(char *poss)
#else
answer(poss)
	char	*poss;
#endif
{
	/* The routines calling this have already done the lock. */

	char	reply[100] ;
	char	*choice ;

	if ( fgets(reply,sizeof reply,i_talkfile)==NULL ) {
		return -2 ;
	}
	if ( reply[0]=='\n' ) return 0 ; /* The first is the default */
	choice= strchr(poss,reply[0]) ;
	if ( !choice ) return -1 ; /* Unknow answer */
	return choice-poss ;
}


#ifdef MEMDEBUG
main_cleanup() {
	/* Free all mem allocated */
	register struct	pathlist	*flist ;
	register struct	pathlist	*fnext ;
	register struct	serverlist	*slist ;
	register struct	serverlist	*snext ;

	if ( ( act==OUT || act==RESCAN || act==NAMES || act==PRIV ) ) {
		for ( flist=xlist ; flist ; flist=fnext ) {
			cs_free(&flist->caps) ; fnext=flist->next ;
			free(flist) ;
		}
		xlist= (struct pathlist *)0 ;
		for ( flist=files ; flist ; flist=fnext ) {
			cs_free(&flist->caps) ; fnext=flist->next ;
			free(flist) ;
		}
		files= (struct pathlist *)0 ;
		for ( slist=arch_slist ; slist ; slist=snext ) {
			snext=slist->next ;
			free(slist) ;
		}
		arch_slist= (struct serverlist *)0 ;
	}

}
#endif
