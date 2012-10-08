/*	@(#)io.c	1.1	96/02/27 13:15:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* STARCH - STore and reSTore ARCHives of graphs.
   module implementing reading and writing archive blocks.

*/
/* The read and write routines to archives possibly run in a separate
   thread. Therefore they should only write and read variables local
   to this module. They can read global variables set at initialization
   time that will not change. The buffering itself is done in block.c.
   The io done here should only be to stderr and through the routines
   warning and fatal.
 */

#include "starch.h"
#include "block.h"
#include "io.h"
#include <module/tape.h>
#include <module/disk.h>
#ifdef __STDC__
#include <time.h>
#else
#include <posix/time.h>
#endif

/* Checktape results, EXCEPTION can also be returned  */
#define	CHK_RETRY	0
#define CHK_DOTAIL	1
#define CHK_END		2
#define CHK_EOF		3
#define CHK_OK		4

long			volno ;		/* Volume number */
long			seqno ;		/* Sequence number */

capset			arch_cs ;	/* capset for archive */
capability		arch_cap ;	/* cap for archive */
capset			arch_dir ;	/* the capability with the dir entry */
int			arch_valid ;	/* The archive capability is valid */

static disk_addr offset_medium; /* Current disk offset */
unsigned long	sz_medium ;	/* Total size of medium */
unsigned long	tot_rblocks ;	/* Total # of blocks read */
unsigned long	tot_wblocks ;	/* Total # of blocks written */

/* volume signalling */
static int	do_load;	/* execute load_tape before r/w */
static int	do_eov;		/* Put out an EVOL in next block */
				/* 0 -> no eov
				   1 -> seen eov
				   2 -> handling eov
				 */

static int	do_sov;		/* At start of tape */
static int	do_stop;	/* No more reads */



char		*r_format ;	/* format read from tape */
char		*r_version ;	/* version read from tape */
char		*r_sdate ;	/* date read from tape */
long		r_date ;	/* date read from tape */

long		dumptime ;	/* Date written to tape */
char		*dumpdate ;	/* Date written to tape */

/* Flag indicating usage of stdio for the archive */
int		do_stdio ;

/* Block allocation admin data */
byte	*cur_unit ;	/* Start of thingie to read/write in one go */
b_fsize	unit_size ;	/* Read buffer size, might be >NBLOCKS */
int	rw_nblocks = 20; /* # 1024 byte blocks in cur_unit */
int	rw_blno ;	/* Currently used block */
int	max_w_blno ;	/* Size of current buffer */

/* read admin */
static byte	*end_unit;	/* just past end of data read in */
static byte	*next_bl;	/* first byte to be considered next */
static byte	*next_search;	/* first byte for search_magic */

extern char	*ctime();

/* List of archives that were opened */
struct f_head_t seen_archs ;

#ifdef __STDC__
static void	prepnw(void);
static void	buf_setchk(byte *);
static void	setvs(byte *,int);
#else
static void	prepnw();
static void	buf_setchk();
static void	setvs();
#endif
void		send_eof();


static void
setvs(buf,nblks)
	byte		*buf;
	int		nblks ;
{
	int		i;

	for ( i=0 ; i<nblks ; i++ ) {
		buf_setchk(buf+i*BLOCK_SIZE) ;
	}
}

/* Check whether we have seen the archive name before ..*/
/* And remember it if not. */
static int
seen_before(name)
	char *name ;
{
	struct f_list_t		*cur ;

	for(cur=seen_archs.first ; cur ; cur=cur->next ) {
		if ( strcmp(cur->path,name)==0 ) return 1 ;
	}
	f_insert(&seen_archs,name,IO_FILE);
	return 0 ;
}

/* Openarch, try to open a named archive.
*/
int
#ifdef __STDC__
openarch(char *name, int wr_flag, int reopen_flag, int mt_flag)
#else
openarch(name,wr_flag,reopen_flag, mt_flag)
	char	*name ;
	int	wr_flag ;
	int	reopen_flag ;
	int	mt_flag ;
#endif
{
	errstat		errcode;
	int		done ;
#ifdef AMOEBA
#ifdef NOTDEF
	capability	*cap ;
	char		*type ;
#endif
#endif

	done=0 ;
	do_stdio=0 ;
	if ( strcmp("-",name)==0 ) {
#ifdef NOTDEF
		type=(wr_flag?"STDOUT":"STDIN") ;	
		cap=getcap(type) ;	
		if ( cap==NULL ) {
			t_fatal(mt_flag,
				"could not find environment capability for %s",
				type) ;
			return EXCEPTION ;
		}
		do_stdio=1 ;
		done=1 ;
		arch_cap= *cap ;
		result=cs_singleton(&arch_cs,cap) ;
		if ( !result ) {
			t_fatal(mt_flag,"cs_singleton failed");
			return EXCEPTION ;
		}
		arch_valid=1 ;
#else
		do_stdio=1 ;
		done=1 ;
#endif
	}
	if ( !done ) {	
		cs_free(&arch_cs);
		errcode=sp_lookup(SP_DEFAULT,name,&arch_cs) ;
		if ( errcode!=STD_OK ) {
			if ( reopen_flag ) {
				warning("can not access %s",name) ;
				return NOT_OK ;
			}
			t_fatal(mt_flag,"can not access %s",name) ;
			return EXCEPTION ;
		}
		errcode=cs_to_cap(&arch_cs,&arch_cap) ;
		if ( errcode!=STD_OK ) {
			if ( reopen_flag ) {
				warning("can not access %s",name) ;
				return NOT_OK ;
			}
			t_fatal(mt_flag,"could not get capability for archive: \"%s\"",
					err_why(errcode));
			return EXCEPTION ;
		}
		arch_valid=1 ;
		do_load= seen_before(name) ;
	}
	return OK ;
}

void
#ifdef __STDC__
init_wio(void)
#else
init_wio()
#endif
{
	time_t	clock ;
	long	secs ;
	int	msec,tz,dst ;
	errstat	err ;
	char	*cptr ;

	rw_blno= 0 ;

	volno=1 ;
	seqno=1 ;

	offset_medium= 0 ;

	prepnw();

	if ( !unit_size ) {
		/* do this one or zero times */
		unit_size= rw_nblocks*BLOCK_SIZE ;
		cur_unit=(byte *)getmem((unsigned)unit_size) ;
	}

	if ( !dumpdate ) {
		/* date */
		err=tod_gettime(&secs,&msec,&tz,&dst);
		if ( err!=STD_OK ) {
			warning("can not get time, assuming 1 Jan 1970 00:00:00") ;
			secs=0 ;
		}
		dumptime=secs ;
		clock=secs ;
		dumpdate=ctime(&clock);
		for ( cptr=dumpdate ; *cptr ; cptr++ ) {
			if ( *cptr=='\n' ) {
				*cptr=0 ;
				break ;
			}
		}
	}
}

#ifdef __STDC__
void	aio_hdr(struct ba_list	*hdr_io,int type)
#else
void	aio_hdr(hdr_io,type)
	struct ba_list	*hdr_io ;
	int		type ;
#endif
{
	wio_start(hdr_io) ;
	wio_1b(hdr_io,AR_HDR) ;
	wio_1b(hdr_io,type);
	wio_string(hdr_io,ARCHID);
	wio_string(hdr_io,ARCHVERS);
	wio_string(hdr_io,dumpdate);
	wio_4b(hdr_io,dumptime) ;
	wio_end(hdr_io) ;
}

int
load_tape(write_flag) 
	int		write_flag ;
{
	errstat		err ;
	int		re_opend=0 ;
	static char	device[100] ;
	int		openstat ;

#ifdef MULTI_THREADED
	err_lock();
#endif
	ini_talk();
	do {
	
		fprintf(o_talkfile,"Please insert tape");
		if ( ar_type ) fprintf(o_talkfile," for %s", ar_type) ; 
		fprintf(o_talkfile," in %s and hit return when ready\n",
			archive) ; 
		fprintf(o_talkfile,"Tape(default), new Device or Abort(t/d/a)? ");
		switch( answer("tda") ) {
		case -2 : t_fatal(1,"could not get answer");
			  return EXCEPTION ;
		case -1 : continue ;
		case 2 :
			fprintf(o_talkfile,"Are you sure you want to abort(y/n)? ");
			if ( answer("y")==0 ) {
#ifdef MULTI_THREADED
				err_unlock();
#endif
				t_fatal(1,"user abort") ;
				return EXCEPTION ;
			}
			continue ;
		case 1 :
			fprintf(o_talkfile,"Please give the name of the device[%s]: ",archive);
			fgets(device,sizeof device,stdin) ;
			if ( !(device[0]==0 || device[0]=='\n') ) {
				/* Take the plunge and use the new name */
				archive=device ;
			}
			openstat= openarch(archive, write_flag, 1, 1) ;
			if ( openstat==NOT_OK ) continue ;
			if ( openstat== EXCEPTION ) return EXCEPTION ;
			break ;		
		}
		do_sov=1 ;
		do_load=0 ;

		err=tape_load(&arch_cap) ;
		if ( !re_opend && err==STD_NOTFOUND ) {
			switch ( openarch(archive,write_flag,1,1) ) {
			case EXCEPTION :
				return EXCEPTION ;
			case OK  :
				re_opend=1 ;
				  continue ;
			}
		}
		if ( err!=STD_OK ) {
			fprintf(o_talkfile,"Tape load failed \"%s\"\n",
				tape_why(err));
		}
	} while ( err!=STD_OK ) ;
#ifdef MULTI_THREADED
	err_unlock();
#endif
	offset_medium=0 ;
	return OK ;
}

void
unload_tape() {
	errstat		err ;

	err=tape_unload(&arch_cap);
	if ( err!=STD_OK ) warning("tape_unload failed %s",tape_why(err));
	do_load=1 ;
	offset_medium=0 ;
}

void
fudge_curse(buf)
	byte		*buf;
{
	/* Code used when media are switched after a write error.
	   The dirty trick used is to overwrite all volno's, seqno's &
	   checksum's in each block.
	   MT-safe.
	*/

	volno++ ;
	seqno=1 ;
	
	setvs(buf,rw_nblocks);


}

int
checktape(err,buf,sz_todo,sz_done,write_flag)
	errstat		err;
	ptr		buf ;
	long		sz_todo;
	long		sz_done;
	int		write_flag;
{
	int		reply;

	if ( err== -15008 && sz_done==0 && write_flag ) {
		warning("tape error\"%s\" assuming End-Of-Tape",
				tape_why(err));
		fudge_curse(buf) ;
		unload_tape() ;
		if ( load_tape(write_flag)==EXCEPTION ) return EXCEPTION  ;
		return CHK_RETRY ;
	}
	if ( err==TAPE_EOF && sz_done>0 ) {
		return CHK_EOF ;
	}
	if ( ( write_flag && sz_done!=sz_todo ) || sz_done<=0 ) {
		if ( write_flag ) {
			if ( err==TAPE_EOT ) {
				if ( do_eov==0 ) do_eov++ ;
				return CHK_DOTAIL ;
			}
#ifdef MULTI_THREADED
			err_lock();
#endif
			ini_talk();
			if ( do_sov ) {
				fprintf(o_talkfile,
					"%s: Tape error \"%s\" at start of tape %s\n",
					progname,
					tape_why(err),archive) ;
			} else {
				fprintf(o_talkfile,
					"%s: Tape error \"%s\" at offset %ld on %s\n",
					progname,
					tape_why(err),offset_medium,archive) ;
			}
		} else {
#ifdef MULTI_THREADED
			err_lock();
#endif
			fprintf(o_talkfile,"Tape error \"%s\" at offset %ld on %s\n",
			tape_why(err),offset_medium,archive) ;
		}
		do {
			if ( write_flag ) {
				fprintf(o_talkfile,"Retry, Abort, Next tape (r/a/n)? ");
			} else {
				fprintf(o_talkfile,"Retry, Abort, Next tape, End (r/a/n/e)? ");
			}
			reply= answer(write_flag?"ran":"rane") ;
		} while (reply==-1 ) ;
		switch ( reply ) {
		case 0:
#ifdef MULTI_THREADED
			err_unlock();
#endif
			return CHK_RETRY ;
		case 1:
#ifdef MULTI_THREADED
			err_unlock();
#endif
			t_fatal(1,"User Abort") ;
			return EXCEPTION ;
		case 2: if ( write_flag ) {
			    if ( do_sov ) {
				fprintf(o_talkfile,"The tape you just removed contains no information\n") ;
			    }
			    fudge_curse(buf) ;
			}
#ifdef MULTI_THREADED
			err_unlock();
#endif
			unload_tape() ;
			if (load_tape(write_flag)==EXCEPTION) return EXCEPTION ;
			return CHK_RETRY ;
		case -2:
			if ( write_flag ) {
				t_fatal(1,"could not get answer");
				return EXCEPTION ;
			}
			warning("could not get answer, assuming end");
		case 3:
#ifdef MULTI_THREADED
			err_unlock();
#endif
			return CHK_END ;
		}
	}
#ifdef MULTI_THREADED
	err_unlock();
#endif
	if ( err==TAPE_EOT ) {
		if ( do_eov==0 && write_flag ) do_eov++ ;
		return CHK_OK ;
	}
	warning("non-fatal tape error (%s)",tape_why(err)) ;
	return CHK_OK ;
}

b_fsize
rw_tape(buf,size,write_flag)
	ptr		buf ;
	long		size ;
	int		write_flag ;
{
	bufsize		sz_iot ;
	errstat		err ;
	bufptr		iobuf ;
	bufsize		iosize ;
	b_fsize		total_iosize ;

	iobuf=(bufptr)buf ; iosize=size ;
	total_iosize= 0 ;
	if ( do_load ) {
		if ( load_tape(write_flag)==EXCEPTION ) return EXCEPTION ;
	}
	for (;;) {
		if ( write_flag ) {
			err=tape_write(&arch_cap,iobuf,iosize,&sz_iot);
		} else {
			if ( do_stop ) {
				if ( do_stop==1 ) {
					err=TAPE_EOF ; sz_iot=0 ;
					do_stop=0 ;
				} else {
					return total_iosize ;
				}
			} else {
				err=tape_read(&arch_cap,iobuf,iosize,&sz_iot);
			}
		}
		total_iosize += sz_iot ;
		if ( err==STD_OK ) {
			break ;
		}
		switch ( checktape(err,buf,size,(long)sz_iot,write_flag) ) {
		case CHK_DOTAIL :	iobuf += sz_iot ; iosize -= sz_iot ;
		case CHK_RETRY :	continue ;
		case CHK_END :		do_stop=2 ; return total_iosize ;
		case CHK_EOF :		do_stop=1 ; return total_iosize ;
		case CHK_OK :		break ;
		default:
					t_fatal(1,"internal, checktape");
		case EXCEPTION:
					return EXCEPTION ;
		}
		break ;
	}

	do_sov=0 ;
	return total_iosize ;
}

int
load_flop(write_flag)
	int	write_flag ;
{
	static char	device[100];

#ifdef MULTI_THREADED
	err_lock();
#endif
	ini_talk();
	for(;;) {
		fprintf(o_talkfile,"next Floppy(%s), new Device or Abort(f/d/a)? ",
			archive);
		switch (answer("fda")) {
		case -2 : t_fatal(1,"could not get answer");
			return EXCEPTION ;
		case -1 : continue;
		case 2 :
			fprintf(o_talkfile,"Are you sure you want to abort(y/n)? ");
			if (answer("y") == 0) {
#ifdef MULTI_THREADED
				err_unlock();
#endif
				t_fatal(1,"user abort");
				return EXCEPTION ;
			}
			continue ;
		case 1 :
			fprintf(o_talkfile,"Please give the name of the device[%s]: ",
					archive);
			fgets(device,sizeof device,stdin);
			if (!(device[0] == '\0' || device[0] == '\n')) {
				/* Take the plunge and use the new name */
				archive=device;
			}
			switch ( openarch(archive, write_flag, 1, 1) ) {
			case OK:
				continue ;
			case EXCEPTION:
				return EXCEPTION ;
			}
			break ;		
		}

		do_sov=1;
		do_load=0;

		break ;
	}
#ifdef MULTI_THREADED
	err_unlock();
#endif
	offset_medium=0 ;
	return OK ;
}

void
unload_flop() {
	do_load=1 ;
	offset_medium=0 ;
}

int
checkflop(err, buf, sz_todo, write_flag)
	errstat		err;
	ptr		buf;
	long		sz_todo;
	int		write_flag;
{
	if (sz_todo > 0) {
#ifdef MULTI_THREADED
		err_lock();
#endif
		ini_talk();
		if (err != STD_ARGBAD) {
			fprintf(o_talkfile,
				"%s: Floppy error \"%s\" at offset %ld on %s\n",
				progname,
				err_why(err), offset_medium,archive);
		} else
			fprintf(o_talkfile,
				"%s: End of floppy disk %s at block %ld\n",
				progname,
				archive,offset_medium/BLOCK_SIZE);

		for (;;) {
			if (write_flag)
				fprintf(o_talkfile,
				   "Retry, Abort, Next floppy (r/a/n)? ");
			else
				fprintf(o_talkfile,
				   "Retry, Abort, Next floppy, End (r/a/n/e)? ");

			switch (answer(write_flag ? "ran" : "rane")) {
			case 0:
#ifdef MULTI_THREADED
				err_unlock();
#endif
				return CHK_RETRY;
			case 1:
#ifdef MULTI_THREADED
				err_unlock();
#endif
				t_fatal(1,"User Abort");
				return EXCEPTION ;
			case 2: if ( write_flag ) {
				    if ( do_sov ) {
fprintf(o_talkfile,"The floppy you just removed contains no information\n") ;
				    }
				    fudge_curse(buf) ;
				} else {
				    volno++ ; seqno=0 ;
				}
#ifdef MULTI_THREADED
				err_unlock();
#endif
				if ( load_flop(write_flag)==EXCEPTION ) {
					return EXCEPTION ;
				}
				return CHK_RETRY;
			case -2 : 
				if ( write_flag ) {
					t_fatal(1,"could not get answer");
					return EXCEPTION ;
				}
				warning("could not get answer, assuming end");
			case 3:
#ifdef MULTI_THREADED
				err_unlock();
#endif
				return CHK_END;
			}
		}
	}
#ifdef MULTI_THREADED
	err_unlock();
#endif
	return CHK_OK;
}

b_fsize
rw_flop(buf,size,write_flag)
	byte		*buf;
	long		size;
	int		write_flag;
{
	errstat		err;
	disk_addr	nblocks ;
	disk_addr	block ;

	nblocks= size/BLOCK_SIZE ;

	if (do_load) {
		if ( load_flop(write_flag)==EXCEPTION ) return EXCEPTION ;
	}
	for (;;) {
		block= offset_medium/BLOCK_SIZE ;
		if (write_flag) {
			err = disk_write(&arch_cap, LOG2_BLOCK_SIZE,
				block, nblocks,
				(bufptr)buf);
		} else {
			if (do_stop) return 0;
			err = disk_read(&arch_cap, LOG2_BLOCK_SIZE,
				block, nblocks,
				(bufptr)buf);
		}
		if ( err==STD_OK ) break ;

		switch (checkflop(err, buf, size, write_flag)) {
		case CHK_RETRY:
			continue;
		case CHK_END:
			do_stop = 1;
			return 0;
		case CHK_OK:
			break ;
		default:
			t_fatal(1,"internal, checkflop");
		case EXCEPTION:
			return EXCEPTION ;
		}
		break;

	}
	do_sov = 0;
	return size;
}

/* Checks whether we are near to the end of a volume in normal writing mode.
   If so, sets the size of the buffer to be written to the remiaining number
   if blocks (if <rw_nblocks) and triggers EOV handling.
 */
static
#ifdef __STDC__
void prepnw(void)
#else
void prepnw()
#endif
{
	if ( !do_eov &&
	     sz_medium && offset_medium+unit_size+BLOCK_SIZE>sz_medium ) {
		do_eov=1 ;
		max_w_blno= (sz_medium-offset_medium)/BLOCK_SIZE ;
	} else {
		max_w_blno= rw_nblocks ;
	}
}

int
wdata(last)
	int		last;
{
	errstat		err ;
	capability	newcap ;
	int		flags=0 ;
	b_fsize		towrite ;
	b_fsize		didwrite ;

	towrite= rw_blno*BLOCK_SIZE ;
	if ( sz_medium && offset_medium+towrite>sz_medium ) {
		if ( io_type!=IO_FILE ) {
			t_fatal(1,"internal: end of medium handling") ;
			return EXCEPTION ;
		}
		t_fatal(1,"can not write past size given") ;
		return EXCEPTION ;
	}
	/* Set volume and sequence numbers and checksum */
	setvs(cur_unit,rw_blno);
	switch ( io_type ) {
	case IO_TAPE:
	case IO_TAPE_NR:
		if ( towrite ) {
			towrite= max_w_blno*BLOCK_SIZE ;
			if ( rw_tape(cur_unit,(long)towrite,1)==EXCEPTION ) {
				return EXCEPTION ;
			}
			if ( last ) {
				send_eof() ; send_eof() ;
				if ( io_type!=IO_TAPE_NR ) {
					err=tape_rewind(&arch_cap,0) ;
					if (err!=STD_OK ) {
						warning("could not rewind tape") ;
					}
				}
			}
		}
		break ;
	case IO_FLOP:
		if ( towrite && rw_flop(cur_unit,(long)towrite,1)==EXCEPTION ) {
				return EXCEPTION ;
		}
		break ;
	case IO_FILE:
		if ( do_stdio ) {
			didwrite=fwrite((char *)cur_unit,1,(int)towrite,stdout) ;
			if ( didwrite!=towrite ) {
				t_fatal(1,"error while writing on stdout") ;
				return EXCEPTION ;
			}
		} else {
			if ( last ) flags=BS_SAFETY|BS_COMMIT ;
			for (;;) {
				err=b_modify(&arch_cap,offset_medium,
					(char *)cur_unit,
					towrite,flags,&newcap) ;
				if ( err==STD_OK ) {
					arch_cap=newcap ;
					break ;
				} else if ( err==STD_NOMEM ) {
					warning("error STD_NOMEM while writing archive");
					sleep((unsigned)350) ;
					continue ;
				} else {
					std_destroy(&arch_cap);
					t_fatal(1,"error \"%s\" while writing archive",
						err_why(err));
					return EXCEPTION ;
				}
			}
		}
		break ;
	default:
		t_fatal(1,"internal: unknow io_type");
		return EXCEPTION ;
	}
	offset_medium+= towrite;
	tot_wblocks+= rw_blno ;
	return OK ;
}

void
send_eof() {
	errstat		err ;

	err=tape_write_eof(&arch_cap) ;
	if ( err!=STD_OK && err!=TAPE_EOT ) {
		warning("error %s while write tape mark",
				tape_why(err)) ;
	}
}

int
#ifdef __STDC__
out_block(struct ba_list  *my_buffer)
#else
out_block(my_buffer)
	struct ba_list  *my_buffer ;
#endif
{
	if ( my_buffer->flags&BA_END ) {
		/* End of write actions. Flush the buffer */
		return wdata(1) ;
	}

	if ( rw_blno<0 ) {
		t_fatal(1,"internal: sending unknown block");
		return EXCEPTION ;
	}
	memcpy(cur_unit+rw_blno*BLOCK_SIZE,my_buffer->content,BLOCK_SIZE);
	rw_blno++ ;
	if ( rw_blno >= max_w_blno ) {
		if ( wdata(0)==EXCEPTION ) return EXCEPTION ;
		rw_blno=0 ;
		prepnw() ;
	}
	/* The following code inserts extra blocks into the stream. It uses
	   the buffer just copied to the output buffer.
	 */
	if ( do_eov==1 && rw_blno>=max_w_blno-1 ) {
		/* We are voluntarely ending this volume and want to insert
		   an EVOL as the last block of the current buffer.
		 */
		/* This flushes the tape */
		aio_hdr(my_buffer,AR_EVOL) ;
		/* I would like to use the buffer pool mechanism, but we
		   might be single threaded...
		 */
		do_eov=2 ; /* Inhibit eov handling for out_block */
		if ( out_block(my_buffer) == EXCEPTION ) return EXCEPTION ;
		if ( io_type==IO_TAPE || io_type==IO_TAPE_NR ) {
			send_eof() ; send_eof() ;
		}
		message("%s: End of volume %ld",progname,volno) ;
		volno++ ;
		seqno=1 ;
		switch( io_type ) {
		case IO_TAPE:
		case IO_TAPE_NR:
			unload_tape();
			break ;
		case IO_FLOP:
			unload_flop();
			break ;
		}
		do_eov=0 ;
		prepnw() ;
	}
	if ( ( (seqno+rw_blno)%1000 == 0 || (seqno==1 && rw_blno==0 ) ) &&
	     do_eov==0 ) {
		/* Insert starch hdr's once in a while */
		aio_hdr(my_buffer,AR_IGNORE) ;
		my_buffer->flags= 0 ;
		/* I would like to use the buffer pool mechanism, but we
		   might be single threaded...
		 */
		if ( out_block(my_buffer) == EXCEPTION ) return EXCEPTION ;
	}
	return OK ;
}

#ifdef __STDC__
unsigned	chk_sum(byte *addr)
#else
unsigned	chk_sum(addr)
	byte			*addr ;
#endif
{
	register unsigned	sum ;
	register int		i ;
	register byte		*next ;
	/* Calculate checksum */
	sum=0 ;
	for ( i=BLOCK_SIZE/2, next=addr ; i ; i-- ) {
		sum += byteval(next[1]) + (((unsigned)byteval(next[0]))<<8);
		next +=2 ;
	}
	return sum&0xFFFF;	/* Checksum */
}

/* ----------------------------------------------------------------------
   All of the previous was devoted to writing blocks.
   The next part will be devoted to reading blocks.
 */

void
#ifdef __STDC__
init_rio(void)
#else
init_rio()
#endif
{
	errstat		err;

	if ( io_type==IO_TAPE || io_type==IO_TAPE_NR ) {
		err=tape_load(&arch_cap) ;
		if  ( err!=STD_OK ) {
			fatal("could not load tape unit (%s)",
				tape_why(err));
		}
	}
	if ( io_type==IO_FLOP ) {
		do_sov=1;
		do_load=0;
		offset_medium=0 ;
	}
	unit_size= rw_nblocks*BLOCK_SIZE ;
	cur_unit=(byte *)getmem((unsigned)unit_size) ;
	end_unit=cur_unit ;
	next_bl= cur_unit ; /* Past end */
	volno=1 ; seqno=0 ;
}

static int rdata()
{
	register byte	*src, *dst ;
	register int	n ;
	errstat		err ;
	long		toread ;
	b_fsize		didread ;
	b_fsize		totread ;
	long		save ;

	/* First shift all unconsidered material down */
	n= end_unit-next_bl ;
	src=next_bl ; dst=cur_unit ;
	while ( n-- ) *dst++ = *src++ ;
	next_bl=cur_unit ; end_unit=dst ;
	totread=0 ;
	/* Then read a buffer after it */
	do {
		if ( sz_medium && offset_medium+unit_size>sz_medium ) {
			toread=sz_medium-offset_medium ;
			if ( toread<=0 ) {
				if ( io_type==IO_TAPE || io_type==IO_TAPE_NR ) {
					if ( load_tape(0)==EXCEPTION ) {
						return EXCEPTION ;
					}
					continue ;
				}
				if ( io_type==IO_FLOP ) {
					if ( load_flop(0)==EXCEPTION ) {
						return EXCEPTION ;
					}
					continue ;
				}
				t_fatal(1,"can not read past end of medium") ;
				return EXCEPTION ;
			}
		} else {
			toread=unit_size ;
		}
		if ( end_unit-cur_unit>toread ) {
			/* Buffer is becoming too small */
			save= end_unit-cur_unit ;
			cur_unit=adjmem(cur_unit,(unsigned)(2*unit_size));
			next_bl=cur_unit ;
			end_unit= cur_unit+save ;
		}
		switch ( io_type ) {
		case IO_TAPE:
		case IO_TAPE_NR:
			didread=rw_tape(end_unit,toread,0) ;
			if ( didread<=0 ) return didread ; /* also EXCEPTION */
			break ;
		case IO_FLOP:
			didread=rw_flop(end_unit,toread,0) ;
			if ( didread<=0 ) return didread ; /* also EXCEPTION */
			break ;
		case IO_FILE:
			if ( do_stdio ) {
				didread=fread(end_unit,1,(int)toread,stdin) ;
			} else {
				err=cs_b_read(&arch_cs,(b_fsize)offset_medium,
						(char *)end_unit,
						(b_fsize)toread,&didread) ;
				if ( err!=STD_OK ) {
					t_fatal(1,
						"read error \"%s\" on archive",
						err_why(err));
					return EXCEPTION ;
				}
			}
			break;
		default:
			t_fatal(1,"internal: unknown io_type");
			return EXCEPTION ;
		}
		if ( didread==0 ) {
			warning("unexpected EOF on archive");
			return NOT_OK ;
		}
		offset_medium += didread ;
		end_unit +=didread ;
		totread+= didread ;
	} while ( next_bl+BLOCK_SIZE>end_unit ) ;
	tot_rblocks+= totread/BLOCK_SIZE ;
	return OK ;
}

int search_magic() {
	static int	warn_count ;
	byte		magic_stuff[4] ;
	byte		*srch_ptr ;
	register	int i ;
	register	int cmp_val ;
	long		val = HDR_CONSTANT ;
	
	if ( warn_count < 20 ) {	
		warning("could not find magic constant, starting to search..");
	}
	if ( warn_count==20 ) {
		warning("no more warnings will be issued for searches for the magic constant") ;
	}
	warn_count ++ ;
	/* Set up search buffer for constant */
	i= 4 ; srch_ptr=magic_stuff+i ;
	while ( i-- ) {
		*--srch_ptr= val & 0xFF ;
		val >>= 8 ;
	}
	/* Now start searching for the 0th, 1th, 2nd and 3rd byte */
	srch_ptr=next_search+8 ; /* Just past vol & seq */
	cmp_val=byteval(magic_stuff[0]) ;	   /* Looking for a first */
	for(;;) {
		if ( srch_ptr +12 >= end_unit ) {
			/* Threatens to read past end-of-buffer */
			next_bl=srch_ptr ;
			i=rdata() ;
			if ( i!=OK ) return i ;
			srch_ptr= next_bl ;
		}
		if ( srch_ptr +4 >= end_unit ) {
			return NOT_OK ;
		}
		if ( byteval(*srch_ptr)==cmp_val ) {
			for ( i=1 ; i<4 ; i++ ) {
				if ( srch_ptr[i]!=magic_stuff[i] ) break ;
			}
			if ( i==4 ) break ; /* Found!! */
		}
		srch_ptr++;
	}
	next_bl= srch_ptr - 8 ;
	return OK ;
}

	
static int
#ifdef __STDC__
r1_block(struct ba_list *my_buffer)
#else
r1_block(my_buffer)
	struct ba_list *my_buffer ;
#endif
{
	long		t_volno ;
	long		t_seqno ;
	long		t_magic ;
	int		did_search=0 ;
	long		skipped=0 ;

#define skipmsg	if ( skipped ) warning("skipped %ld description blocks",skipped)
	for (;;) {
		while ( next_bl+BLOCK_SIZE>end_unit ) {
			/* need to read data */
			switch ( rdata() ) {
			case EXCEPTION:
				return EXCEPTION ;
			case NOT_OK :
				skipmsg ;
				return NOT_OK ;
			} /* OK is the remaining case */
		}
		next_search=next_bl+1 ;
		memcpy(my_buffer->content,next_bl,BLOCK_SIZE) ;
		my_buffer->rw_pos=0 ;
		next_bl += BLOCK_SIZE ;
		/* Now check for the magic */
		t_volno=rio_4b(my_buffer);
		t_seqno=rio_4b(my_buffer);
		t_magic=rio_4b(my_buffer);
	
		if ( t_magic==HDR_CONSTANT ) {
			/* Test checksum */
			/* skip it first */
			rio_2b(my_buffer);
			if ( chk_sum(my_buffer->content)!=0 ) {
				warning("incorrect checksum in volume %d block %ld, skipping block",
						t_volno,t_seqno) ;
				if ( did_search ) {
					/* continue searching */
					if ( !search_magic() ) {
						skipmsg ;
						return 0 ;
					}
				}
				continue ;
			}
			/* test volume number */
			if ( t_volno != volno ) {
				int	reply ;
#ifdef MULTI_THREADED
				err_lock();
#endif
				ini_talk();
				do {
					fprintf(o_talkfile,"%s: expected volume %ld got %ld.\n" ,
							progname,volno, t_volno) ;
					fprintf(o_talkfile,"do you want continue with this volume(n/y)? ");
					fflush(o_talkfile) ; /* No risks taken */
					reply= answer("ny") ;
					if ( reply== -2 ) {
						t_fatal(1,"could not get answer");
					}
				} while ( reply<0 ) ;
#ifdef MULTI_THREADED
				err_unlock();
#endif
				if ( reply==0 ) {
					switch ( io_type ) {
					case IO_TAPE:
					case IO_TAPE_NR:
						unload_tape() ;
						load_tape(0) ;
						break ;
					case IO_FLOP:
						unload_flop() ;
						load_flop(0) ;
						break;
					default:
						t_fatal(1,"user abort") ;
					}
					continue ; /* retry */
				}
				volno= t_volno ;
			}
			if ( t_seqno<seqno+1 ) {
				skipped++ ;
				continue ;
			}
			break ; /* OK */
		}
		/* The magic is not there, search for it and retry */
		if ( !search_magic() ) {
			skipmsg ;
			return 0 ;
		}
		did_search=1 ;
	}
	skipmsg ;
	if ( t_seqno!=seqno+1 ) {
		warning("missing %ld description blocks",t_seqno-seqno-1) ;
	}
	seqno=t_seqno ;
	return 1 ;
}

void
chk_hinfo(my_buffer)
	struct ba_list	*my_buffer;
{
	char		*l_format ;
	char		*l_version ;
	char		*l_sdate ;

	l_format=rio_string(my_buffer) ;
	if ( !r_format ) r_format=l_format ;
	if ( strcmp(l_format,r_format)!=0 ) {
		warning("inconsistent headers, id's differ: %s and %s",
			l_format, r_format );
	}
	l_version=rio_string(my_buffer) ;
	if ( !r_version ) r_version=l_version ;
	if ( strcmp(l_version,r_version)!=0 ) {
		warning("inconsistent headers, versions differ: %s and %s",
			l_version, r_version );
	}
	l_sdate=rio_string(my_buffer) ;
	if ( !r_sdate ) r_sdate= l_sdate ;
	if ( strcmp(l_sdate,r_sdate)!=0 ) {
		warning("inconsistent headers, dates differ: %s and %s",
			l_sdate, r_sdate );
	}
}

void
#ifdef __STDC__
in_block(struct ba_list *my_buffer)
#else
in_block(my_buffer)
	struct ba_list *my_buffer ;
#endif
{
	int		type ;
	static int	first=1 ;
	static int	eof=0 ;
	int		saved_pos ;

	my_buffer->flags=0 ;
	for(;;) {
		if ( eof ) {
			my_buffer->flags= BA_END ;
			return ;
		}
		switch( r1_block(my_buffer) ) {
		case NOT_OK:
			my_buffer->flags= BA_ERROR ;
			return ;
		case EXCEPTION:
			my_buffer->flags= BA_ABORT ;
			return ;
		}

		saved_pos= my_buffer->rw_pos ; /* For higher up */
		type=rio_1b(my_buffer);
		if ( type!=AR_HDR ) {
			if ( first ) {
				warning("missing initial header") ;
				first=0 ;
			}
			my_buffer->rw_pos= saved_pos ;
			return ;
		}
		switch ( rio_1b(my_buffer) ) {
		case AR_IGNORE:
			chk_hinfo(my_buffer);
			break ;
		case AR_START:
			if ( !first ) {
				warning("unexpected initial header") ;
			}
			first=0 ;
			r_format=keepstr(rio_string(my_buffer)) ;
			if ( strcmp(r_format,ARCHID)!=0 ) {
				warning("expected id \"%s\", got \"%s\"",
					ARCHID,r_format) ;
			}
			r_version=keepstr(rio_string(my_buffer)) ;
			if ( strcmp(r_version,ARCHVERS)!=0 ) {
				warning("expected version \"%s\", got \"%s\"",
					ARCHVERS,r_version) ;
			}
			r_sdate=keepstr(rio_string(my_buffer)) ;
			if ( verbose ) {
				message("archive created on %s",r_sdate) ;
			}
			r_date=rio_4b(my_buffer) ;
			break ;
		case AR_EVOL:
			message("%s: End of volume %ld", progname,volno) ;
			chk_hinfo(my_buffer);
			if ( io_type==IO_TAPE || io_type==IO_TAPE_NR ) {
				unload_tape();
			}
			if ( io_type==IO_FLOP ) {
				unload_flop();
			}
			volno++ ; seqno=0 ;
			break ;
		case AR_END:
			chk_hinfo(my_buffer);
			if ( in_multi && f_next(&arch_list) )  {
				if (openarch(archive, 0, 0, 1)==EXCEPTION) {
					my_buffer->flags= BA_ABORT ;
					return ;
				}
				first=1 ;
				volno=1 ; seqno=0 ;
				do_sov=1 ;
				offset_medium=0 ;
				end_unit=cur_unit ;
				next_bl= cur_unit ; /* Past end */
			} else {
				eof=1 ;
			}
			break ;
		default:
			warning("unrecognized header type") ;
		}
	}
}

int chk_med_sz(attempt) int attempt ; {
        if ( sz_medium<10*BLOCK_SIZE ) {
            if ( attempt ) return 0 ;
            fatal("medium size must be >= %d\n",10*BLOCK_SIZE) ;
        }
        if ( sz_medium<rw_nblocks*BLOCK_SIZE ) {
            if ( attempt ) return 0 ;
            fatal("medium size must be >= buffer size") ;
        }
        /* adjust sz_medium down to multiple of blocks */
        if ( sz_medium % BLOCK_SIZE ) {
            if ( !attempt ) {
                    warning("medium size is not multiple of %d",BLOCK_SIZE) ;
            }
            sz_medium -= sz_medium%BLOCK_SIZE ;
        }
	return 1 ;
}

void
get_disksize(attempt)
	int attempt ;	/* Do not complain if attempt==1 and things fail */
{
	errstat		err ;
	disk_addr	d_size ;

	/* size in program call arguments overrides */
	if ( sz_medium ) return ;
	err=disk_size(&arch_cap,LOG2_BLOCK_SIZE,&d_size) ;
	if ( err!=STD_OK ) {
		if ( !attempt ) {
			warning("could not determine size of medium (disk_size failed)");
		}
		return ;
	}	
	sz_medium=d_size*BLOCK_SIZE ;
	if ( sz_medium ) {
	    chk_med_sz(attempt) ;
	}
	
}

void
chk_sz(sz_buf)
	unsigned long	sz_buf ;
{
	/* called by decargs */

	if ( sz_buf ) {
	    if ( sz_buf%BLOCK_SIZE ) {
		fatal("buffer size must be a multiple of %d\n",BLOCK_SIZE) ;
	    }
	    rw_nblocks=sz_buf/BLOCK_SIZE ;
	}
	if ( sz_medium && !chk_med_sz(0) ) {
		sz_medium=0 ;
	}
}

/* Set vol & seqno in a single block */
static void
buf_setchk(buf)
	byte		*buf ;
{
	struct ba_list	l_io ;

	l_io.rw_pos=0 ; l_io.content= buf ;
	wio_4b(&l_io,volno) ;
	wio_4b(&l_io,seqno) ;
	l_io.rw_pos= CHK_OFF ;
	wio_2b(&l_io,0);		/* Goal */
	l_io.rw_pos= CHK_OFF ;
	wio_2b(&l_io,-chk_sum(buf));	/* Checksum */
	seqno++;
}

void
rw_stats() {
	if ( tot_rblocks ) {
		message("Number of bytes read from archive:  %ldKb",tot_rblocks);
	}
	if ( tot_wblocks ) {
		message("Number of bytes written to archive: %ldKb",tot_wblocks);
	}
}
