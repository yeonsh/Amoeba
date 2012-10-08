/*	@(#)msg5.c	1.2	92/06/11 10:37:01 */
/*
 *			M S G 5 . C
 *
 * Functions -
 *	setup		read mailbox file
 *	newmessage	notice & read new mail
 *	overwrit	remove deleted messages from a file by overwriting
 *	undigestify	process an undigestify command
 *	nambinary	build path name for binary parallel file
 *	binbuild	Write out a fresh binary parallel file
 *	binupdate	Update one record of the binary file
 *	binheaderupdate	Update the header of the binary file
 *
 *
 *		R E V I S I O N   H I S T O R Y
 *
 *	09/13/82  MJM	This module split off from msg1.c, to even
 *			the sizes and ease editing.
 *
 *	09/22/82  MJM	Added in most of the support for the parallel
 *			binary map file.
 *
 *	03/16/83  MJM	Added binary update routines, neatened manipulation
 *			of binary box overall.  Faster, too!
 *
 *	11/10/83  DPK	Added Steve Kille (SEK) continuation line fix.
 *
 *	07/15/84  HAW	Made binarybox and memory structures identical.
 *			Added readonly mode.
 */

#include "util.h"
#include "mmdf.h"
#include "pwd.h"
#include "signal.h"
#include "sys/stat.h"
#include "./msg.h"
#ifdef V4_2BSD
#include "sys/file.h"
#include "strings.h"
#else V4_2BSD
#include "string.h"
#endif V4_2BSD

#ifndef V4_2BSD
sigtype	(*oldhup)();
sigtype	(*oldintr)();
sigtype	(*oldquit)();
#endif

#ifndef W_OK
#define	W_OK 2
#endif

extern FILE *lk_fopen();

/*
 *			S E T U P
 *
 *	This function is called to scan the contents of the mailbox.
 */
setup( setflg)
int     setflg;
{
	char    line[LINESIZE];
	char	name[LINESIZE];
	char	mbox[LINESIZE];
	char    datestr[M_BSIZE];	/* where to save Date field */
	char	fromstr[M_BSIZE];	/* where to save From field */
	char	sndstr[M_BSIZE];	/* where to save Sender field */
	char	tostr[M_BSIZE];		/* where to save To field */
	char	subjstr[M_BSIZE];	/* where to save Subj field */
	char	curhdr[M_BSIZE];	/* Scratch for gothdr() */
	char	digsubject[SIZESUBJ];	/* Scratch for undigest function */
	char	*cpy, *cpz;
	int	needscan;
	unsigned int     inmsg;
	unsigned int    count;
	long    newpos;
	long    curpos;
	int     oldflag = 0;
	int	oldsig;
	static int i;		/* String index */
	char systemtemp[1024];

	needscan = inmsg = 0;

	switch( setflg )  {

	case SETAPND:
		status.ms_time = statb2.st_mtime;
		fseek( filefp, status.ms_eofpos, 0);
		newpos = status.ms_eofpos;
		break;

	case SETREAD:
		if( filefp != NULL)  {
			++oldflag;
			fclose( filefp);
		}
		if( (filefp = fopen( filename, "r")) == NULL)  {
			printf( "can't open '%s'\r\n", filename);
			if( oldflag)  {
				strcpy( filename, oldfile);
				filefp = fopen( filename, "r");
			}
			error( "");
		}
		if( binfp != (FILE *)NULL ) {
		        lk_fclose( binfp, binarybox, (char *)0, (char *)0);
			binfp = (FILE *)NULL;
		}

		fstat( fileno( filefp), &statb1);

		if( statb1.st_size == 0 ) {		/* No mail - exit */
			if( quickexit == ON ) {
				printf("%s is empty!\n",filename);
				tt_norm();
				exit(0);
			}
		}

		status.ms_time = statb1.st_mtime;
		if( (statb1.st_mode & S_IFMT) != S_IFREG)
			error( "File is not a regular file.\r\n");

		if( access( filename, W_OK ) < 0 ) {
			printf(" Unable to write %s - READONLY mode\r\n",filename);
			readonly = TRUE;
		}

		/* Set flags if this is the default mail file */
		mainbox();

		/* Build the name of the binary mail index file */
		nambinary( filename );

		if( (binfp = fopen( binarybox, "r" )) != NULL )  {

#ifndef V4_2BSD
			oldhup = signal(SIGHUP,SIG_IGN);
			oldintr = signal(SIGINT,SIG_IGN);
			oldquit = signal(SIGQUIT,SIG_IGN);
#else
			oldsig = sigsetmask(SPSIGS);
#endif
			binaryvalid = FALSE;

			/* Load binary map first */
			if( bprint == ON ) {
				printf("Loading binary box %s ", binarybox );
				fflush( stdout );
			}
			/* Read header */
			fread( &status, sizeof(struct status), 1, binfp );

			/* Quick hack - change from OLDIDENTITY to
			 * IDENTITY can be done by msg */
			if( status.ms_ident == OLDIDENTITY ) {
				rewind(binfp);
				fread( &status, sizeof(struct oldstatus),
					1, binfp );
				status.ms_markno = 0;
				/* binbuild will be called below */
			} else if( status.ms_ident != IDENTITY )  {
				tt_norm();
				printf("\n\nBinary file version error.");
				printf("  file=%x, program=%x\n", status.ms_ident, IDENTITY );
				printf("This version of msg uses a new binary file which is not compatible\n");
				printf("with the old version. You may either delete the binary\n");
				printf("file %s and restart msg\n", binarybox );
				printf("or use the old version of msg. Deleting the binary file\n");
				printf("will destroy the old status flags but no mail will be lost.\n");
 				printf("The old version may still be used by typing msg.bak but may disappear\n");
				printf("soon so plan to convert to the new version as soon as possible.\n");

				exit(10);
				/* NOTREACHED */
			}

			/* Read data */
			for( i = 0; i < status.ms_nmsgs; i++ )  {
				if( (msgp[i] = (struct message *) malloc(sizeof(struct message)) ) == NULL ) {
					status.ms_nmsgs = i-1;	
					nomem();
					/* NOTREACHED */
				}

				if(fread( msgp[i], sizeof(struct message), 1, binfp ) != 1) {
					tt_norm();
					printf("\nThe binary box is corrupted.\n");
					printf("rm %s and retry 'msg'.\n", binarybox);
					exit(11);
				}

				if( bdots == ON && bprint == ON ) {
					putchar('.');
					fflush(stdout);
				}
			}
			/* Quick check of binary box integrity:
			 * check for delimiters in last message */
			fseek( filefp, status.ms_eofpos-strlen( delim2 ), 0 );
			if( ( xfgets( line, 2+strlen( delim2 ), filefp ) == NULL )
			    || (strequ( line, delim2 ) == FALSE ))  {
				tt_norm();
				printf("\nThe binary box is corrupted.\n");
				printf("rm %s and retry 'msg'.\n", binarybox);
				exit(11);
			}
			
			if( bprint == ON ) {
				printf(" - Done\r\n");
				fflush( stdout );
			}

			if( status.ms_ident == OLDIDENTITY ) {
				printf("Rebuilding binary box\r\n");
				binbuild();
			}

			/* Proceed with appending the rest of the messages */
			newpos = status.ms_eofpos;

			fclose( binfp );
			binfp = (FILE *)NULL;
			binaryvalid = TRUE;
#ifndef V4_2BSD
			signal(SIGHUP,oldhup);
			signal(SIGINT,oldintr);
			signal(SIGQUIT,oldquit);
#else
			sigsetmask(oldsig);
#endif

			/* Fall through to read any new messages */
			setflg = SETAPND;	/* to get "new" flags */

		}  else  {

			printf( "Reading %s ", filename);
			fflush( stdout);

			status.ms_curmsg = 0;		/* Global */
			status.ms_nmsgs = 0;		/* Global */
			newpos = status.ms_eofpos = 0;

			if( readonly == FALSE )
				if( (i = creat( binarybox, sentprotect )) < 0 )  {
					printf("Unable to create %s - READONLY mode\r\n", binarybox );
					readonly = TRUE;
				}
				else{
				  close(i);
				  sprintf(systemtemp,"chm ff:0:0 %s",binarybox);
				  system(systemtemp);
				}
		}

		if( readonly == FALSE ) {
			/* get exclusive control of file */
			if (access(binarybox, W_OK) < 0 ) {
				printf("Can't write %s - READONLY mode\n",binarybox);
				readonly = TRUE;
			}
			else if ((binfp = lk_fopen(binarybox, "r+", (char *)0,
  					     (char *)0, 5)) == (FILE *) NULL) {
				printf("%s busy - READONLY mode\n",binarybox);
				readonly = TRUE;
			}
		}
		break;

	case SETUNDIGEST:
		fseek( filefp, mptr->start, 0);
		newpos = mptr->start;
		strncpy( digsubject, mptr->subject, SIZESUBJ );
		break;

	default:
		error( "Bad setup()" );
	}

	fseek( filefp, newpos, 0 );
	while( xfgets( line, sizeof( line), filefp) != NULL)  {
		curpos = newpos;
		count = strlen( line);
		newpos += count;

		/*
		 * Check for Message Separators  (Two types)
		 */
		if( strequ( line, delim1) || strequ( line, delim2))  {
			if( setflg == SETUNDIGEST )
				return;
			if( inmsg != 0 )  {
				/* wants message NUMBER */
				binupdate( (int)status.ms_nmsgs );
				status.ms_eofpos = newpos;
			}
			inmsg = 0;
			continue;
		}

		if(
			setflg == SETUNDIGEST &&
			strncmp( line, "--------------------", 20 ) == 0
		)  {
			inmsg = 0;
			continue;
		}

		/*
		 * Note each new message.  If we are not in a message, and
		 * a line of non-separator has been found, start off a new
		 * message.
		 */
		if( inmsg == 0)  {
			if( mdots == ON ) {
				putchar( '.' );
				fflush( stdout);
			}
			/* Clear out header scan area */
			datestr[0] =
			    fromstr[0] =
			    sndstr[0] =
			    tostr[0] =
			    subjstr[0] = '\0';
			curhdr[0] = '\0';	/* SEK continuation fix! */

			/* scan off leading blank lines */
			while( count == 1)  {
				xfgets( line, sizeof( line), filefp);
				curpos = newpos;
				count = strlen( line);
				newpos += count;
			}

			/* Build new table entry */
			if( status.ms_nmsgs >= Nmsgs)  {
				printf("More than %d messages in file.\r\n", Nmsgs);
				status.ms_nmsgs--;
				nomem();
				/* NOTREACHED */
			}

			/*
			 * Allocate a new msgp slot, either at the end of the
			 * messages, or in the middle, depending.
			 */
			if( setflg != SETUNDIGEST )  {
				if( (mptr = msgp[status.ms_nmsgs++] = (struct message *) malloc(sizeof(struct message)) ) == NULL ) {
					status.ms_nmsgs--;
					nomem();
					/* NOTREACHED */
				}
				mptr->flags = 0;
			}  else  {
				/*
				 * scrunch down.
				 *   msgno is current message NUMBER
				 *   status.ms_nmsgs is last message NUMBER.
				 * offsets are one less than numbers.
				 */
				if( (mptr = msgp[status.ms_nmsgs] = (struct message *) malloc(sizeof(struct message)) ) == NULL ) {
					error("Out of memory - operation aborted\n");
					/* NOTREACHED */
				}
				
				for( i = status.ms_nmsgs-1; i >= msgno; i-- )  {
					msgp[i+1] = msgp[i];
				}
				status.ms_nmsgs++;
				msgp[ msgno++ ] = mptr;
				mptr->flags = M_NEW;
			}

			mptr->start = curpos;
			mptr->len = 0L;
			mptr->date = 0L;
			mptr->from[0] = mptr->datestr[0] = mptr->subject[0] = mptr->to[0] = '\0';

			needscan = 1;
			inmsg++;
		}
		mptr->len += count;

		if( count == 1 && needscan)  {
			/*
			 * Blank line found while reading header.
			 * Digest header information and store results
			 * into msgp[] structure entry.
			 */

			/* Process date field( if any) */
			if( !isnull( datestr[0])) {
				if( (mptr->date = smtpdate( datestr )) != -1L)
					makedate( &mptr->date, mptr->datestr);
			}

			/* Process to field ( if any ) */
			if( !isnull( tostr[0] ) ) {
				strncpy( mptr->to, tostr, SIZETO );
				mptr->to[SIZETO-1] = '\0';
			}

			/* Process from field( if any) */
			if( !isnull( fromstr[0]))  {
				parsadr( fromstr, name, mbox, (char *)0);

				if( prefix( username, mbox) && !isnull( tostr[0]))  {
					/* sender was self */
					parsadr(tostr, name, mbox,( char *) 0);
					if( !isnull( name[0]))
						sprintf( fromstr, "To: %s%c", name, '\0');
					else
						sprintf( fromstr, "To: %s%c", mbox, '\0');
				}  else  {
					if( !isnull( name[0]))
						strncpy(fromstr,name,SIZEFROM);
					else
						strncpy(fromstr,mbox,SIZEFROM);
				}
				mptr->from[SIZEFROM-1] = '\0';
				/* Try to eliminate UUCP prefixes */
				{
					register char *cp;
					char * indent;

					indent = cp = fromstr;
					while( *cp )
						if( *cp++ == '!' )
							indent = cp;

					strncpy(mptr->from,indent,SIZEFROM-1);
				}

				mptr->flags |= M_NEW;
			}

			/* Process Subject field, if any */
			if( !isnull( subjstr[0]))  {
				subjstr[M_BSIZE-1] = '\0';
				if( (cpz = index(subjstr,'\n')) != 0 )
					*cpz = '\0';
				if( filoutflag == ON ) {
					for(cpz = subjstr,cpy = mptr->subject;
					    (*cpz != '\0') && (cpy - mptr->subject < SIZESUBJ);
					    cpz++ ) {
						if( *cpz < ' ' || *cpz == '\177' ) {
							*cpy++ = '^';
							*cpy++ = '@' + *cpz;
						}
						else
							*cpy++ = *cpz;
					}
					*cpy = '\0';
				}
				else
					strncpy( mptr->subject, subjstr, SIZESUBJ );
				mptr->subject[SIZESUBJ-1] = '\0';
			}

			/* Header scan complete */
			needscan = 0;
		}

		/* This line is part of the header -- scan it */
		if( needscan && line[0] != '\n' )  {
			gothdr( curhdr, "date:", line, datestr);
			gothdr( curhdr, "from:", line, fromstr);
			gothdr( curhdr, "sender:", line, sndstr);
			gothdr( curhdr, "To:", line, tostr);
			gothdr( curhdr, "subject:", line, subjstr);
		}
	}
	fstat( fileno( filefp), &statb1);
	status.ms_time = statb1.st_mtime;

	/*
	 * Check for having read a message while it was being delivered.
	 * Exclusive opens might be better, but for now...
	 */
	if( inmsg != 0 )  {
		printf("(partial message ignored) ");
		free( (char *) msgp[--status.ms_nmsgs]);
	}

	printf( " %d message%c total.\r\n", status.ms_nmsgs, status.ms_nmsgs == 1 ? ' ' : 's');

	/*
	 * rewrite entire binary file if we expanded the middle of the
	 * table with an undigestify operation.  SETREAD and SETAPND handled
	 * inside loop, with a binupdate() after each new message.
	 */
	if( readonly == FALSE ) {
		if( setflg == SETUNDIGEST )
			binbuild();
		else
			binheaderupdate();
		fflush( binfp );
	}
}

/*
 *			N E W M E S S A G E
 *
 * notice & include new mail
 */
newmessage()
{
	/* Might also be useful to check the sizes */

/*	if( filefp == NULL ||
	    fstat( fileno( filefp), &statb2) < 0 ||*/
        if (stat(filename,&statb2) < 0 ||
	    statb2.st_mtime == status.ms_time
	) 
	  return;		/* nothing new */

	fclose(filefp);
	filefp=(FILE *)fopen(filename,"r");
	printf( "Reading new messages ");
	setup( SETAPND );		/* incorporate appended messages */
}

/*
 *			V P U T M S G
 *
 * This routine gives a visual indication of each message being written
 * out, with one dot per message.  Makes the user feel better about the
 * often longish wait.
 *
 * This routine is really intended only for the use of the overwrite function,
 * as it records the NEW position of of the message in the file, to avoid
 * guesswork about message lengths in overwrite().
 */
vputmsg()
{
	long pos, ftell();
	char tbuf[1024];
	register int tt;
		
	if( mdots == ON ) {
		putchar('.');
		fflush( stdout );
	}

	fputs( delim1, outfp );

	pos = ftell( outfp );
	if( mptr->flags & M_RESTMAIL ) {
		/* Write the rest of the mail file */
		fseek(filefp, mptr->start, 0);
		while( (tt = fread(tbuf, sizeof(char), sizeof(tbuf), filefp)) == sizeof(tbuf) )
			fwrite(tbuf, sizeof(char), strlen(tbuf), outfp);
		fwrite(tbuf,sizeof(char), tt, outfp);
	}
	else {
		writmsg();
		fputs( delim2, outfp );
	}
	mptr->start = pos;
}

/*
 *			O V E R W R I T
 *
 * remove deleted messages from file.
 *
 * Note that vputmsg() above updates the message start position info.
 */
overwrit()
{
	static char tempfile[128];	/* build name of temporary file */
	unsigned int ndeleted;
	unsigned int i,ii;
	static char *ptr;
	register struct message **inp, **outp;	/* For mashing table */
	char systemtemp[1024];
	
	if( readonly == TRUE )
		error("READONLY mode - overwrite ignored\n");

	for( ndeleted = 0, i = status.ms_nmsgs; i-- != 0; ) {
		if( msgp[i]->flags & M_RESTMAIL )
			msgp[i]->flags &= ~M_DELETED;

		if( msgp[i]->flags & M_DELETED )
			ndeleted++;		/* how many dead messages */
	}
	
	if( ndeleted == 0)  {
		printf( "file unchanged, so not updated\r\n");
		return;
	}

	if( outmem == FALSE && stat( filename, &statb2) >= 0 && status.ms_time != statb2.st_mtime)  {
			/* Leave the processing to newmessage() */
			/* We can't just return here, because this is called
			 * from the "exit" command...
			 */
			error( "File updated since last read...\r\n");
	}

	/* get exclusive control of file */
	if( (exclfd = lk_open( filename, 1, (char *)0, (char *)0, 5))
	     < 0 && errno == EBUSY)  {
		printf( "'%s' busy; try later", filename);
		error( "\r\n");
	}

	if( ndeleted == status.ms_nmsgs)  {
		/*
		 *  All the messages are deleted.
		 *  Truncate (always if mailbox), otherwise delete
		 */
		printf("All messages deleted from %s.\r\n", filename );
		if( ismainbox)  {
			creat( filename, sentprotect);
			sprintf(systemtemp,"chm ff:0:0 %s",filename);
			system(systemtemp);
/*LISA temp.*/
	/*		creat( binarybox, sentprotect );*/
		}  else  {
			printf( "delete %s", filename);
			/* for safety... */
			if( confirm((char *)0,DOLF))  {
			  lk_close(exclfd,filename,NULL,NULL);
			  exclfd = -1;
			  unlink( filename );
			  unlink( binarybox );
			}
			else {
				creat( filename, sentprotect);
				sprintf(systemtemp,"chm ff:0:0 %s",filename);
				system(systemtemp);
		/*		creat( binarybox, sentprotect );
				sprintf(systemtemp,"chm ff:0:0 %s",binarybox);
				system(systemtemp);*/
			}
		}
	}  else  {
		strcpy( tempfile, filename);
		/* get the path to directory of file  */
		if (ptr = rindex(tempfile, '/'))
			*++ptr = '\0';
		else
			ptr = tempfile;
		strcpy(ptr, tempname);
		mktemp(tempfile);

		if( (outfd = creat( tempfile, sentprotect)) < 0)
			error( "can't create temporary file\r\n");

		if( (outfp = fdopen( outfd, "w" )) == NULL )
			error( "can't fdopen temp file\n" );
		outfd = -1;

		/* save the undeleted messages */
		printf("OverWriting %s ", filename);
		unset();
		settype( 'u' );
		doiter( vputmsg );
		status.ms_eofpos = ftell(outfp);
		fclose( outfp );

		lk_close(exclfd,filename,NULL,NULL);
		exclfd = -1;
		if( unlink( filename) < 0)
			error( "can't delete old file\r\n");
		if( link( tempfile, filename) < 0)  {
			printf( "can't rename temporary file '%s' to be '%s'.\r\n",
			tempfile, filename);
			error( "The temporary file still exists.\r\n");
		}
		sprintf(systemtemp,"chm ff:0:0 %s",filename);
		system(systemtemp);

/*LISA temp*/
/*		unlink( tempfile);*/
	}
	if( exclfd >= 0 ) {
		lk_close( exclfd, filename, NULL, NULL);
		if( stat( filename, &statb2 ) >= 0 )
			status.ms_time = statb2.st_mtime;
	}
unlink(tempfile);
	/* Mash table down to reflect new file layout */
	inp = &msgp[0];
	outp = &msgp[0];
	ndeleted = 0;
	i = status.ms_curmsg;
	ii = status.ms_markno;
	for( msgno = 1; msgno <= status.ms_nmsgs; msgno++ )  {
	
		if( (*inp)->flags & M_DELETED )  {
			ndeleted++;
			free(*inp++);
			if( msgno < i )
				status.ms_curmsg--;
			if( msgno < ii )
				status.ms_markno--;
		}  else  {
			/* Note that position changes are done by vputmsg() */
			/*
			 * If the input and output slots are different,
			 * clean up the input slot.  (Else we clobber
			 * ourselves!).
			 */
			if( (*inp) != (*outp) )  {
				*outp = *inp;
			}
			inp++;
			outp++;
		}
	}

	/* Adjust current message so as not to run off end */
	status.ms_nmsgs -= ndeleted;
	if( status.ms_curmsg > status.ms_nmsgs )
		status.ms_curmsg = status.ms_nmsgs;

	if( status.ms_markno > status.ms_nmsgs )
		status.ms_markno = status.ms_curmsg;
		
	if( status.ms_nmsgs == 0 ) {
		status.ms_curmsg = 0;
		return;
	}

	fclose( filefp );
	if( (filefp = fopen( filename, "r" )) == NULL )
		error("Unable to re-open file");

	fstat( fileno( filefp ), &statb1 );
	status.ms_time = statb1.st_mtime;

	printf( " %d message%c total.\r\n", status.ms_nmsgs, status.ms_nmsgs == 1 ? ' ' : 's');

	if( outmem == TRUE ) {
		/* Alter the status struct so the rest of the msgs will
		 *  be recovered by newmessage()
		 */
		for( i = status.ms_nmsgs; i-- != 0; )
			if( msgp[i]->flags & M_RESTMAIL )
				status.ms_eofpos = msgp[i]->start;
		status.ms_nmsgs--;
		status.ms_time = 0;
	}

	/*
	 * At this point, the rest of the program should not even
	 * know that anything has happened.  Things should be in
	 * a good enough state that everything else works normally.
	 */
	/* Update the binary file */
	binbuild();
}

/*
 *			U N D I G E S T I F Y
 */
undigestify()
{
	/* mptr set by doiter for setup() */
	delmsg();
	setup( SETUNDIGEST );
}


/*
 *			N A M B I N A R Y
 *
 * Build the name of the associated binary file, and save the name
 * in the string "binarybox".
 */
nambinary( arg )
register char *arg;
{
	register char *sav, *cp;
	char ending[32];
	char fname[72];

	ending[0] = fname[0] = 0;
	strcpy( fname, arg );

	/* Find the last path component */
	sav = cp = fname;
	while( *cp )
		if( *cp++ == '/' )
			sav = cp;

	if( sav == fname )  {
		/* No slashes found in path name */
		sprintf( binarybox, "%s%s", binarypre, fname );
	}  else  {
		strcpy( ending, sav );
		*(sav-1) = 0;

		sprintf( binarybox, "%s/%s%s", fname, binarypre, ending );
	}
}

/*
 *			B I N B U I L D
 *
 * Rewrite the entire binary file, including the header.
 */
binbuild()
{
	static char tempfile[128];	/* build name of temporary file */
	register int i;
	char *ptr;
	int oldsig;
	char systemtemp[1024];

	if( readonly == TRUE )
		return;

	if( binfp != (FILE *) NULL ) {
		lk_fclose( binfp, binarybox, (char *)0, (char *)0 );
		binfp = (FILE *)NULL;
	}

	/* Don't bother making a new binary file if mailbox is empty */
	if( status.ms_nmsgs <= 0 )  {
		unlink( binarybox );
		return;
	}

#ifndef V4_2BSD
	oldhup = signal(SIGHUP,SIG_IGN);
	oldintr = signal(SIGINT,SIG_IGN);
	oldquit = signal(SIGQUIT,SIG_IGN);
#else
	oldsig = sigsetmask(SPSIGS);	/* Block all signals */
#endif
	strcpy( tempfile, binarybox);
	/* get the path to directory of file  */
	if (ptr = rindex(tempfile, '/'))
		*++ptr = '\0';
	else
		ptr = tempfile;
	strcpy(ptr, tempname);
	mktemp(tempfile);

	if( (i = creat( tempfile, sentprotect )) < 0 )
		error("Unable to create temporary file\r\n");

	if( (binfp = fdopen( i, "w" )) == NULL )
			error( "can't fdopen temp file\n" );

	if( bprint == ON ) {
		printf("Writing binary file:");
		fflush (stdout);
	}

	binheaderupdate();

	for( i=0; i < status.ms_nmsgs; i++ )  {
		fwrite( msgp[i], sizeof(struct message), 1, binfp );
		if( bdots == ON && bprint == ON ) {
			putchar( '.' );
			fflush( stdout );
		}
	}
	fclose( binfp );
	if( unlink( binarybox ) < 0)
			error( "can't delete old binarybox\r\n");
	if( link( tempfile, binarybox ) < 0)  {
		printf( "can't rename temporary file '%s' to be '%s'.\r\n",
			tempfile, binarybox);
		error( "The temporary file still exists.\r\n");
	}
	sprintf(systemtemp,"chm ff:0:0 %s",binarybox);
	system(systemtemp);

	unlink( tempfile);

	if( (binfp = lk_fopen( binarybox, "r+", (char *)0, 
					  (char *)0, 5 )) == (FILE *) NULL )
		error("Can't re-open binary box\r\n");

	if( bprint == ON )
		printf(" done\r\n");
	fflush( stdout );
#ifndef V4_2BSD
	signal(SIGHUP,oldhup);
	signal(SIGINT,oldintr);
	signal(SIGQUIT,oldquit);
#else
	sigsetmask(oldsig);	/* restore signals */
#endif
}

/*
 *			B I N U P D A T E
 *
 * Update a single message in the binary box.
 * The number given is in the range 1...nmsgs.
 */
binupdate( number )
int number;
{
	long place;

	if( readonly == TRUE )
		return;

	if( number <= 0 || number > Nmsgs )
		printf("binupdate(%d): out of range\n", number);

	if( binfp == (FILE *) NULL )  {
		printf("binupdate: opening binary box\n");
		if( (binfp = lk_fopen( binarybox, "r+", (char *)0,
					  (char *)0, 5 )) == (FILE *) NULL ) {
			printf("binupdate: unable to open binary box");
			fflush(stdout);
			return;
		}
	}	

	place = (number-1) * sizeof(struct message) + sizeof(struct status);
	if( fseek( binfp, place, 0) < 0 )
		printf("binupdate: fseek error\n");

	fwrite( msgp[number-1], sizeof(struct message), 1, binfp );
}

/*
 *			B I N H E A D E R U P D A T E
 */
binheaderupdate()
{
	if( readonly == TRUE )
		return;

	/* Build header record */
	fseek( binfp, (long) 0, 0 );
	status.ms_ident = IDENTITY;
	fwrite( &status, sizeof(struct status), 1, binfp );
}

/*			N O M E M
 *  
 *  Recover gracefully when out of memory
 */
nomem() {

	struct message **sp;
	long ls;

	/* Find the last msg; not always the "last" one */
	ls = msgp[0]->start;
	for( sp = &msgp[1]; sp < &msgp[status.ms_nmsgs]; sp++ ) {
		if( (*sp)->start > ls ) {
			mptr = (*sp);
			ls = (*sp)->start;
		}
	}

	mptr->flags |= M_RESTMAIL;
	outmem = TRUE;
	printf("\n\007***********************************************\n");
	printf("\007Out of memory - Move or delete existing messages then overwrite\n");
	printf("Additional messages will appear after the overwrite!\n");
	error("\007***********************************************\n");
	/* NOTREACHED */
}

setmbox(file)
register char *file;
{
	strcpy(defmbox, file);
	strcpy(defoutfile, file);
	ismainbox++;
}

closeup(status)
register int status;
{
	if (binfp != (FILE *) NULL)
		if (readonly == TRUE)
			fclose(binfp);
		else
			lk_fclose(binfp, binarybox, (char *)0, (char *)0);
	exit(status);

	/* NOT REACHED */
}
