/*	@(#)cpdir.c	1.3	96/03/01 16:52:30 */
/* cpdir - copy directory  	 	Author: Erik Baalbergen */
/* adapted for Amoeba by Kees Verstoep */

/* Use "cpdir [-v] src dst" to make a copy dst of directory src.
   Cpdir should behave like the UNIX shell command
   (cd src; tar cf - .) | (mkdir dst; cd dst; tar xf -)
   
   The -m "merge" flag enables you to copy into an existing directory.
#ifndef AMOEBA
   The -s "similar" flag preserves the full mode, uid, gid and times.
#endif
   The -v "verbose" flag enables you to see what's going on when running cpdir.
   
   Work yet to be done:
   
   - link checks, i.e. am I not overwriting a file/directory by itself?
   
   Please report bugs and suggestions to erikb@cs.vu.nl
   */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#define BUFSIZE 1024
#define PLEN	256

char *prog;
int vflag = 0;	/* verbose */
int mflag = 0;	/* force */
#ifndef AMOEBA
int sflag = 0;	/* similar */
#endif

#ifdef AMOEBA
#define make_similar(sp, d)	/* not supported yet */
#else
#define make_similar(sp, d)	if (sflag) { similar(sp, d); }
#endif

/* Forward */
void cpdir _ARGS((struct stat *, char *, char *));

main(argc, argv)
int argc;
char *argv[];
{
    char *p, *s;
    struct stat st;
    
    prog = *argv++;
    if ((p = *argv) && *p == '-') {
	argv++;
	argc--;
	while (*++p) {
	    switch (*p) {
	    case 'v':
		vflag = 1;
		break;
	    case 'm':
		mflag = 1;
		break;
#ifndef AMOEBA
	    case 's':
		sflag = 1;
		break;
#endif
	    default:
		fatal("illegal flag %s", p);
	    }
	}
    }
    if (argc != 3) {
#ifdef AMOEBA
	fatal("Usage: cpdir [-mv] source destination");
#else
	fatal("Usage: cpdir [-msv] source destination");
#endif
    }
    s = *argv++;
    if (stat(s, &st) < 0)
	fatal("can't get file status of %s", s);
    if (!S_ISDIR(st.st_mode))
	fatal("%s is not a directory", s);
    cpdir(&st, s, *argv);
    exit(0);
}

void
cpdir(sp, s, d)
struct stat *sp;
char *s, *d;
{
    char spath[PLEN], dpath[PLEN];
    register char *send = spath, *dend = dpath;
    DIR *dirp;
    struct stat st;
    struct dirent *dp;

    if ((dirp = opendir(s)) == NULL) {
	nonfatal("can't read directory %s", s);
	return;
    }

    if (mflag == 0 || stat(d, &st) != 0	|| !S_ISDIR(st.st_mode)) {
	makedir(d, 0775);
	make_similar(sp, d);
    }

#ifndef AMOEBA
    if (first) {
	stat(d, &st);
	dev = st.st_dev;
	ino = st.st_ino;
	first = 0;
    }
    if (sp->st_dev == dev && sp->st_ino == ino) {
	nonfatal("%s skipped to avoid an endless loop", s);
	return;
    }
#else
    /* Still to be added: Amoeba specific loop checking code.
     * The stat info is bogus, capabilities should be used instead.
     * The dir_graph module should be used to achieve this.
     */
#endif

    while (*send++ = *s++) {}
    while (*dend++ = *d++) {}
    send[-1] = '/';
    dend[-1] = '/';

    while ((dp = readdir(dirp)) != NULL) {
	/* skip "." and ".." entries */
	if (strcmp(dp->d_name, ".") != 0 &&
	    strcmp(dp->d_name, "..") != 0)
	{
	    strcpy(send, dp->d_name);
	    strcpy(dend, dp->d_name);
		
	    if (stat(spath, &st) < 0) {
		fatal("can't get file status of %s", spath);
	    }

#ifndef AMOEBA
	    if ((st.st_mode & S_IFMT)!=S_IFDIR && st.st_nlink>1)
		if (cplink(st,spath,dpath)==1) continue;
#endif
	    switch (st.st_mode & _S_IFMT) {
	    case _S_IFDIR:
		cpdir(&st, spath, dpath);
		break;
	    case _S_IFREG:
		cp(&st, spath, dpath);
		break;
	    default:
#ifndef AMOEBA
		cpspec(&st, spath, dpath);
#else
		cpcap(spath, dpath);
#endif
		break;
	    }
	}
    }
    (void) closedir(dirp);
}

cp(sp, s, d)
struct stat *sp;
char *s, *d;
{
    char buf[BUFSIZE];
    int sfd, dfd, n;
    
    if (vflag)
	printf("cp %s %s\n", s, d);
    if ((sfd = open(s, 0)) < 0)
	nonfatal("can't read %s", s);
    else {
	if ((dfd = creat(d, sp->st_mode & 0777)) < 0)
	    fatal("can't create %s", d);
	while ((n = read(sfd, buf, BUFSIZE)) > 0)
	    if (write(dfd, buf, n) != n)
		fatal("error in writing file %s", d);
	close(sfd);
	close(dfd);
	if (n)
	    fatal("error in reading file %s", s);
	make_similar(sp, d);
    }
}

nonfatal(s, a)
char *s, *a;
{
    fprintf(stderr, "%s: ", prog);
    fprintf(stderr, s, a);
    fprintf(stderr, "\n");
}

fatal(s, a)
char *s, *a;
{
    nonfatal(s, a);
    exit(1);
}

makedir(s, mode)
char *s;
mode_t mode;
{
    if (vflag) {
	printf("mkdir %s\n", s);
    }
    if (mkdir(s, mode) != 0) {
	fatal("can't create %s", s);
    }
}

#ifdef AMOEBA

#include <amoeba.h>
#include <stderr.h>
#include <module/name.h>

cpcap(s, d)
char *s, *d;
{
    /* The src path doesn't point to a file or directory, so we just try to
     * look it up and append the capability at the destination.
     */
    capability cap;

    if (vflag) {
	printf("copy non file object %s to %s.\n", s, d);
    }
    
    if (name_lookup(s, &cap) == STD_OK) {
	struct stat st;

	if (stat(d, &st) == 0 && S_ISDIR(st.st_mode)) {
	    fprintf(stderr, "%s: destination %s is directory, %s not copied\n",
		    prog, d, s);
	} else {
	    (void) name_delete(d); /* remove destination, if it exists */
	    if (name_append(d, &cap) != STD_OK) {
		fprintf(stderr, "%s: couldn't install %s at %s\n", prog, s, d);
	    }
	    /* similarity flag support? */
	}
    } else {
	nonfatal("couldn't lookup %s", s);
    }
}

#else

#define MKDIR1 "/bin/mkdir"
#define MKDIR2 "/usr/bin/mkdir"

#ifdef __STDC__
mkdir(char *s, mode_t mode)
#else
mkdir(s, mode) char *s; mode_t mode;
#endif
{
    int pid, status;
    
    if ((pid = fork()) == 0) {
	execl(MKDIR1, "mkdir", s, (char *)0);
	execl(MKDIR2, "mkdir", s, (char *)0);
	fatal("can't execute %s or %s", MKDIR1, MKDIR2);
    }
    if (pid == -1)
	fatal("can't fork", prog);
    wait(&status);
    return(status);
}

similar(sp, d)
struct stat *sp;
char *d;
{
    time_t timep[2];
    
    chmod(d, sp->st_mode);
    chown(d, sp->st_uid, sp->st_gid);
    timep[0] = sp->st_atime;
    timep[1] = sp->st_mtime;
    utime(d, timep);
}

cpspec(sp, s, d)
struct stat *sp;
char *s, *d;
{
    if (vflag)
    {
	printf("copy special file %s to %s.", s, d);
	printf(" Major/minor = %d/%d.",
	       sp->st_rdev>>8, sp->st_rdev&0177);
	printf(" Mode = %o.\n", sp->st_mode);
    }
    if (mknod(d, sp->st_mode, sp->st_rdev)<0)
    {
	perror("mknod");
	nonfatal("Cannot create special file %s.\n",d);
    }
    if (sflag)
	similar(sp, d);
}

#define MAXLINKS 512

struct {
    unsigned short ino;
    unsigned short dev;
    char *path;
} links[MAXLINKS];
int nlinks = 0;

cplink(st,spath,dpath)
struct stat st;
char *spath, *dpath;
{
    /* Handle files that are links.
     * Returns 0 if file must be copied.
     * Returns 1 if file has been successfully linked.
     */
    int i;
    int linkent;
    
    linkent = -1;
    for (i=0; i<nlinks; i++)
    {
	if (links[i].dev==st.st_dev
	    && links[i].ino==st.st_ino)
	    linkent=i;
    }
    if (linkent>=0) /* It's already in the link table */
    { /* we must have copied it earlier.
       * So just link to the saved dest path. 	
       * Don't copy it twice.
       */
	if (vflag)
	    printf("ln %s %s\n", links[linkent].path,dpath);
	if (link(links[linkent].path,dpath) < 0)
	    fatal("Could not link to %s\n",dpath);
	return(1); /* Don't try to copy it */
    } else { /* Make an entry in the link table */
	if (nlinks >= MAXLINKS)
	    fatal("Too many links at %s\n",dpath);	
	links[nlinks].dev = st.st_dev;
	links[nlinks].ino = st.st_ino;
	links[nlinks].path = malloc(strlen(dpath)+1);
	if (links[nlinks].path == NULL)
	    fatal("No more memory at %s\n",dpath);
	strcpy(links[nlinks].path,dpath);
	nlinks++;
	/* Go ahead and copy it the first time */
	return(0);
    }
}
#endif
