/*	@(#)create.c	1.3	93/10/07 16:11:37 */
/*
 * Create a tar archive.
 *
 * Written 25 Aug 1985 by John Gilmore, ihnp4!hoptoad!gnu.
 *
 * @ (#)create.c 1.36 11/6/87 Public Domain - gnu
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef V7
#include <fcntl.h>
#endif

#ifndef	MSDOS
#include <pwd.h>
#include <grp.h>
#endif

#ifdef AMOEBA
#include <amoeba.h>
#include <capset.h>
#include <module/dgwalk.h>
#include <cmdreg.h>
#include <stderr.h>
#include <server/soap/soap.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

#define direct dirent
#define header tar_header	/* To avoid the definition of header from
				   amoeba.h */
#define bufsize buffer_size	/* ditto */

#else
#ifdef BSD42
#include <sys/dir.h>
#else
#ifdef MSDOS
#include <sys/dir.h>
#else
/*
 * FIXME: On other systems there is no standard place for the header file
 * for the portable directory access routines.  Change the #include line
 * below to bring it in from wherever it is.
 */
#include "ndir.h"
#endif
#endif
#endif

#ifdef USG
#include <sys/sysmacros.h>	/* major() and minor() defined here */
#endif

/*
 * V7 doesn't have a #define for this.
 */
#ifndef O_RDONLY
#define	O_RDONLY	0
#endif

/*
 * Most people don't have a #define for this.
 */
#ifndef	O_BINARY
#define	O_BINARY	0
#endif

#include "tar.h"
#include "port.h"

extern union record *head;		/* Points to current tape header */
extern struct stat hstat;		/* Stat struct corresponding */
extern int head_standard;		/* Tape header is in ANSI format */

/*
 * If there are no symbolic links, there is no lstat().  Use stat().
 */
#ifndef S_IFLNK
#define lstat stat
#endif

extern void	bzero();
extern void	bcopy();
extern int	errno;

extern void print_header();

union record *start_header();
void finish_header();
void finduname();
void findgname();
char *name_next();
void to_oct();
void dump_file();

static nolinks;			/* Gets set if we run out of RAM */

#ifdef AMOEBA

static void
error(s1, s2)
char *s1, *s2;
{
    fprintf(stderr, "%s %s\n", s1, s2 ? s2 : "");
    exit(1);
}

int
find_dir(paths)
dgw_paths *paths;
{
    errstat	  err;

    err=dgwexpand(paths, dump_file);
    if ( err==STD_NOMEM ) error("Out of memory\n", (char *)NULL);
    return DGW_OK;
}

#endif

void
create_archive()
{
	register char	*p;

	open_archive(0);		/* Open for writing */

#ifdef AMOEBA
    {
	dgw_params	dgwp;
	dgw_paths	*current;
	dgw_paths	*last;
	errstat err;

	dgwp.mode = DGW_ALL;
	dgwp.dodir = find_dir;
	dgwp.testdir = (int (*)())NULL;
	dgwp.doagain = (int (*)())NULL;
	dgwp.servers = (capset **)NULL;
	dgwp.entry = last = (dgw_paths *)0;
	while (p = name_next()) {
	    capset start_cs;

	    if (sp_lookup(SP_DEFAULT, p, &start_cs) != STD_OK) {
		error("Can't look up ", p);
	    }
	    current = (dgw_paths *)malloc(sizeof(dgw_paths));
	    if (current == NULL) {
		error("Out of memory", (char *)NULL);
	    }
	    err = cs_to_cap(&start_cs, &current->cap);
	    if (err != STD_OK) {
		error("cs_to_cap error", (char *)NULL);
	    }
	    current->entry = p;
	    current->next = (dgw_paths *)0;
	    if (last != NULL) {
		last->next = current;
	    } else {
		dgwp.entry = current;
	    }
	    last = current;
	    /* First `find' the top level paths */
	    dump_file(current->entry, &start_cs, (capset *)NULL, "");
	    cs_free(&start_cs);
	}
	err = dgwalk(&dgwp);
	if (err != STD_OK) {
	    error("Graph walk error:", err_why(err));
	}
    }
#else
	while (p = name_next()) {
		dump_file(p, -1);
	}
#endif

	write_eot();
	close_archive();
	name_close();
}		

/*
 * Dump a single file.  If it's a directory, recurse.
 * Result is 1 for success, 0 for failure.
 * Sets global "hstat" to stat() output for this file.
 */

#ifdef AMOEBA
int curdev = -1;

/*ARGSUSED*/
void
dump_file(p, object_cs, parent_cs, base_name)
register char *p, *base_name;
capset *object_cs, *parent_cs;
#else
void
dump_file(p, curdev)
	char	*p;			/* File name to dump */
	int	curdev;			/* Device our parent dir was on */
#endif
{
	union record	*header;
	char type;

	/*
	 * Use stat if following (rather than dumping) 4.2BSD's
	 * symbolic links.  Otherwise, use lstat (which, on non-4.2
	 * systems, is #define'd to stat anyway.
	 */
	if (0 != f_follow_links? stat(p, &hstat): lstat(p, &hstat))
	{
badperror:
		perror(p);
badfile:
		errors++;
		return;
	}

	/*
	 * See if we are crossing from one file system to another,
	 * and avoid doing so if the user only wants to dump one file system.
	 */
	if (f_local_filesys && curdev >= 0 && curdev != hstat.st_dev) {
		annorec(stderr, tar);
		fprintf(stderr,
			"%s: is on a different filesystem; not dumped\n",
			p);
		return;
	}

	/*
	 * Check for multiple links.
	 *
	 * We maintain a list of all such files that we've written so
	 * far.  Any time we see another, we check the list and
	 * avoid dumping the data again if we've done it once already.
	 */
	if (hstat.st_nlink > 1) switch (hstat.st_mode & _S_IFMT) {
		register struct link	*lp;

	case _S_IFREG:			/* Regular file */
#ifdef S_IFCTG
	case S_IFCTG:			/* Contigous file */
#endif
#ifndef AMOEBA
#ifdef _S_IFCHR
	case _S_IFCHR:			/* Character special file */
#endif

#ifdef _S_IFBLK
	case _S_IFBLK:			/* Block     special file */
#endif
#endif /* AMOEBA */

#ifdef _S_IFIFO
	case _S_IFIFO:			/* Fifo      special file */
#endif

		/* First quick and dirty.  Hashing, etc later FIXME */
		for (lp = linklist; lp; lp = lp->next) {
			if (lp->ino == hstat.st_ino &&
			    lp->dev == hstat.st_dev) {
				/* We found a link. */
				hstat.st_size = 0;
				header = start_header(p, &hstat);
				if (header == NULL) goto badfile;
				strcpy(header->header.linkname,
					lp->name);
				header->header.linkflag = LF_LINK;
				finish_header(header);
		/* FIXME: Maybe remove from list after all links found? */
				return;		/* We dumped it */
			}
		}

		/* Not found.  Add it to the list of possible links. */
		lp = (struct link *) malloc( (unsigned)
			(strlen(p) + sizeof(struct link) - NAMSIZ));
		if (!lp) {
			if (!nolinks) {
				fprintf(stderr,
	"%s: no memory for links, they will be dumped as separate files\n");
				tar, nolinks++;
			}
		}
		lp->ino = hstat.st_ino;
		lp->dev = hstat.st_dev;
		strcpy(lp->name, p);
		lp->next = linklist;
		linklist = lp;
	}

	/*
	 * This is not a link to a previously dumped file, so dump it.
	 */
	switch (hstat.st_mode & _S_IFMT) {

	case _S_IFREG:			/* Regular file */
#ifdef S_IFCTG
	case S_IFCTG:			/* Contigous file */
#endif
	{
		int	f;		/* File descriptor */
		int	bufsize, count;
		register long	sizeleft;
		register union record 	*start;

		sizeleft = hstat.st_size;
		/* Don't bother opening empty, world readable files. */
		if (sizeleft > 0 || 0444 != (0444 & hstat.st_mode)) {
			f = open(p, O_RDONLY|O_BINARY);
			if (f < 0) goto badperror;
		} else {
			f = -1;
		}
		header = start_header(p, &hstat);
		if (header == NULL) goto badfile;
#ifdef S_IFCTG
		/* Mark contiguous files, if we support them */
		if (f_standard && (hstat.st_mode & S_IFMT) == S_IFCTG) {
			header->header.linkflag = LF_CONTIG;
		}
#endif
		finish_header(header);
		while (sizeleft > 0) {
			start = findrec();
			bufsize = endofrecs()->charptr - start->charptr;
			if (sizeleft < bufsize) {
				/* Last read -- zero out area beyond */
				bufsize = (int)sizeleft;
				count = bufsize % RECORDSIZE;
				if (count) 
					bzero(start->charptr + sizeleft,
						RECORDSIZE - count);
			}
			count = read(f, start->charptr, bufsize);
			if (count < 0) {
				annorec(stderr, tar);
				fprintf(stderr,
				  "read error at byte %ld, reading %d bytes, in file ",
					hstat.st_size - sizeleft,
					bufsize);
				perror(p);	/* FIXME */
				goto padit;
			}
			sizeleft -= count;
			/* This is nonportable (the type of userec's arg). */
			userec(start+(count-1)/RECORDSIZE);
			if (count == bufsize) continue;
			annorec(stderr, tar);
			fprintf(stderr,
			  "%s: file shrunk by %d bytes, padding with zeros.\n",
				p, sizeleft);
			goto padit;		/* Short read */
		}
		if (f >= 0)
			(void)close(f);

		break;

		/*
		 * File shrunk or gave error, pad out tape to match
		 * the size we specified in the header.
		 */
	padit:
		abort();
	}

#if defined(S_IFLNK) && !defined(AMOEBA)
	case S_IFLNK:			/* Symbolic link */
	{
		int size;

		hstat.st_size = 0;		/* Force 0 size on symlink */
		header = start_header(p, &hstat);
		if (header == NULL) goto badfile;
		size = readlink(p, header->header.linkname, NAMSIZ);
		if (size < 0) goto badperror;
		if (size == NAMSIZ) {
			annorec(stderr, tar);
			fprintf(stderr,
				"%s: symbolic link too long\n", p);
			break;
		}
		header->header.linkname[size] = '\0';
		header->header.linkflag = LF_SYMLINK;
		finish_header(header);		/* Nothing more to do to it */
	}
		break;
#endif

	case _S_IFDIR:			/* Directory */
	{
#ifndef AMOEBA
		register DIR *dirp;
		register struct direct *d;
		int our_device = hstat.st_dev;
#endif
		char namebuf[NAMSIZ+2];
		register int len;

		/* Build new prototype name */
		strncpy(namebuf, p, sizeof (namebuf));
		len = strlen(namebuf);
		while (len >= 1 && '/' == namebuf[len-1]) 
			len--;			/* Delete trailing slashes */
		namebuf[len++] = '/';		/* Now add exactly one back */
		namebuf[len] = '\0';		/* Make sure null-terminated */

		/*
		 * Output directory header record with permissions
		 * FIXME, do this AFTER files, to avoid R/O dir problems?
		 * If old archive format, don't write record at all.
		 */
		if (!f_oldarch) {
			hstat.st_size = 0;	/* Force 0 size on dir */
			/*
			 * If people could really read standard archives,
			 * this should be:		(FIXME)
			header = start_header(f_standard? p: namebuf, &hstat);
			 * but since they'd interpret LF_DIR records as
			 * regular files, we'd better put the / on the name.
			 */
			header = start_header(namebuf, &hstat);
			if (header == NULL)
				goto badfile;	/* eg name too long */
			if (f_standard) {
				header->header.linkflag = LF_DIR;
			}
			finish_header(header);	/* Done with directory header */
		}

#ifndef AMOEBA
		/* Hack to remove "./" from the front of all the file names */
		if (len == 2 && namebuf[0] == '.') {
			len = 0;
		}

		/* Now output all the files in the directory */
		if (f_dironly)
			break;		/* Unless the user says no */
		errno = 0;
		dirp = opendir(p);
		if (!dirp) {
			if (errno) {
				perror (p);
			} else {
				annorec(stderr, tar);
				fprintf(stderr, "%s: error opening directory",
					p);
			}
			break;
		}
		
		/* Should speed this up by cd-ing into the dir, FIXME */
		while (NULL != (d=readdir(dirp))) {
			/* Skip . and .. */
			if (d->d_name[0] == '.') {
				if (d->d_name[1] == '\0') continue;
				if (d->d_name[1] == '.') {
					if (d->d_name[2] == '\0') continue;
				}
			}
			if (d->d_namlen + len >= NAMSIZ) {
				annorec(stderr, tar);
				fprintf(stderr, "%s%s: name too long\n", 
					namebuf, d->d_name);
				continue;
			}
			strcpy(namebuf+len, d->d_name);
			dump_file(namebuf, our_device);
		}

		closedir(dirp);
#endif /* AMOEBA */
	}
		break;

#ifndef AMOEBA
#ifdef _S_IFCHR
	case _S_IFCHR:			/* Character special file */
		type = LF_CHR;
		goto easy;
#endif

#ifdef _S_IFBLK
	case _S_IFBLK:			/* Block     special file */
		type = LF_BLK;
		goto easy;
#endif

#ifdef _S_IFIFO
	case _S_IFIFO:			/* Fifo      special file */
		type = LF_FIFO;
#endif

	easy:

		if (!f_standard) goto unknown;

		hstat.st_size = 0;		/* Force 0 size */
		header = start_header(p, &hstat);
		if (header == NULL) goto badfile;	/* eg name too long */

		header->header.linkflag = type;
		if (type != LF_FIFO) {
			to_oct((long) major(hstat.st_rdev), 8,
				header->header.devmajor);
			to_oct((long) minor(hstat.st_rdev), 8,
				header->header.devminor);
		}

		finish_header(header);
		break;
#endif /* AMOEBA */

	default:
	unknown:
		annorec(stderr, tar);
		fprintf(stderr,
			"%s: Unknown file type; file ignored.\n", p);
		break;
	}
}


/*
 * Make a header block for the file  name  whose stat info is  st .
 * Return header pointer for success, NULL if the name is too long.
 */
union record *
start_header(name, st)
	char	*name;
	register struct stat *st;
{
	register union record *header;

	header = (union record *) findrec();
	bzero(header->charptr, sizeof(*header)); /* XXX speed up */

	/*
	 * Check the file name and put it in the record.
	 */
	while ('/' == *name) {
		static int warned_once = 0;

		name++;				/* Force relative path */
		if (!warned_once++) {
			annorec(stderr, tar);
			fprintf(stderr,
	"Removing leading / from absolute path names in the archive.\n");
		}
	}
	strcpy(header->header.name, name);
	if (header->header.name[NAMSIZ-1]) {
		annorec(stderr, tar);
		fprintf(stderr, "%s: name too long\n", name);
		return NULL;
	}

	to_oct((long) (st->st_mode & ~_S_IFMT),
					8,  header->header.mode);
	to_oct((long) st->st_uid,	8,  header->header.uid);
	to_oct((long) st->st_gid,	8,  header->header.gid);
	to_oct((long) st->st_size,	1+12, header->header.size);
	to_oct((long) st->st_mtime,	1+12, header->header.mtime);
	/* header->header.linkflag is left as null */

#ifndef NONAMES
	/* Fill in new Unix Standard fields if desired. */
	if (f_standard) {
		header->header.linkflag = LF_NORMAL;	/* New default */
		strcpy(header->header.magic, TMAGIC);	/* Mark as Unix Std */
		finduname(header->header.uname, st->st_uid);
		findgname(header->header.gname, st->st_gid);
	}
#endif
	return header;
}

/* 
 * Finish off a filled-in header block and write it out.
 * We also print the file name and/or full info if verbose is on.
 */
void
finish_header(header)
	register union record *header;
{
	register int	i, sum;
	register char	*p;

	bcopy(CHKBLANKS, header->header.chksum, sizeof(header->header.chksum));

	sum = 0;
	p = header->charptr;
	for (i = sizeof(*header); --i >= 0; ) {
		/*
		 * We can't use unsigned char here because of old compilers,
		 * e.g. V7.
		 */
		sum += 0xFF & *p++;
	}

	/*
	 * Fill in the checksum field.  It's formatted differently
	 * from the other fields:  it has [6] digits, a null, then a
	 * space -- rather than digits, a space, then a null.
	 * We use to_oct then write the null in over to_oct's space.
	 * The final space is already there, from checksumming, and
	 * to_oct doesn't modify it.
	 *
	 * This is a fast way to do:
	 * (void) sprintf(header->header.chksum, "%6o", sum);
	 */
	to_oct((long) sum,	8,  header->header.chksum);
	header->header.chksum[6] = '\0';	/* Zap the space */

	userec(header);

	if (f_verbose) {
		/* These globals are parameters to print_header, sigh */
		head = header;
		/* hstat is already set up */
		head_standard = f_standard;
		print_header(stderr);
	}

	return;
}


/*
 * Quick and dirty octal conversion.
 * Converts long "value" into a "digs"-digit field at "where",
 * including a trailing space and room for a null.  "digs"==3 means
 * 1 digit, a space, and room for a null.
 *
 * We assume the trailing null is already there and don't fill it in.
 * This fact is used by start_header and finish_header, so don't change it!
 *
 * This should be equivalent to:
 *	(void) sprintf(where, "%*lo ", digs-2, value);
 * except that sprintf fills in the trailing null and we don't.
 */
void
to_oct(value, digs, where)
	register long	value;
	register int	digs;
	register char	*where;
{
	
	--digs;				/* Trailing null slot is left alone */
	where[--digs] = ' ';		/* Put in the space, though */

	/* Produce the digits -- at least one */
	do {
		where[--digs] = '0' + (char)(value & 7); /* one octal digit */
		value >>= 3;
	} while (digs > 0 && value != 0);

	/* Leading spaces, if necessary */
	while (digs > 0)
		where[--digs] = ' ';

}


/*
 * Write the EOT record(s).
 * We actually zero at least one record, through the end of the block.
 * Old tar writes garbage after two zeroed records -- and PDtar used to.
 */
write_eot()
{
	union record *p;
	int bufsize;

	p = findrec();
	bufsize = endofrecs()->charptr - p->charptr;
	bzero(p->charptr, bufsize);
	userec(p);
}
