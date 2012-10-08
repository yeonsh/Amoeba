/*	@(#)block.h	1.4	96/02/27 13:15:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* The output file is written with 2 kinds of blocks:
	Header 		with header info
	Data		just data, nothing more

   All blocks have a size of 1024, the blocking factor
   can be used to read/write multiple blocks in one go.

   All header blocks are encoded/decoded with special routines that
   allow the programs to ignore byte order on the file:
	rio_string, rbl_4b, rbl_2b, rbl_1b
	wbl_string, wbl_4b, wbl_2b, wbl_1b, wbl_cap

   All integers are written with the least significant byte first.

   Header blocks all start with the following:
	A volume number (4b)
	A sequence number (4b)
	The hex constant 62951413 (4b)
	A checksum (2b)
	A type (1b)
	A subtype (1b)

   This is followed by the data pertinent to each type:
   0	A general header, can be expected at any time;
   1	Graph info, including all information on entries;
   2	Contents info, used to create the capabilities to be inserted
		in directories;
   Type 0 can be anywhere in the file, but the first block must be of
   that type. Type 2 follows type 1, subtypes have no ordering constraints.

0
   Subtypes for header info are:
	0=>ignore, 1=Start of Archive, 2=End of Volume, 3=>End of Archive
   Header info consists of the following:
	string		identifies the starch format "starch"
	string		revision of starch format, currently "1"
	2b		size of capabilities in archive
	string		date of dump
	4b		date of dump (seconds since 1 jan 1970, 00:00:00)

1
   Graph info has subtype 0, 1 and 2. Subtype 1 is used for directories
   already defined by subtype 0 headers. Subtype 2 is used to signal the
   end of graph headers and has no contents.
   The other subtypes take the following form:
	4b		cap # of a directory
     Subtype 0 only:
	  2b		internal server number, refers to 2b definitions
	  4b		date1
	  4b		first part of version number (was date2)
	  4b		second part of version number
			Version number was date3 and always contained a 0.
	  2b		# columns
	  [string]*	column names
	  4b		# rows
     Subtype 0 and 1:
	2b		# elements in sequence
	[
		string	entry name
		4b	date1
		4b	date2
		4b	date3
		[  4b	first rights in dir
		  ...
		]
		4b	internal cap #
		4b	rights in cap
		] ...
	]*	

2  Contents info has several subtypes.
   Subtype 0 is server info, used to define server capabilities, possibly
   used while restoring the capabilities;
   It takes the following form:
	2b		# elements in sequence
	[
		2b	server number, as used in 2a headers
		cap	a capability used to access the server
		1b	first byte of std_info
	]
   Server #0 is by default undefined, storing this number in a definition
   for a file or directory is allowed.
   The `first byte of std_info' information is optional. Zero
   means `unknown'.
   The server numbers are always defined by subtype 0 blocks before being
   used by subtype 1 blocks.
 
   Subtype 1 blocks contain the definitions for one or more non-directory
   capabilities and take the following form:
	1b		id type
			   0=> End of Definitions
			   1=> data follows to define a file
			   2=> capability
			   3=> zombie number
			   4=> empty capset
	4b		cap #
	4b		date1
	4b		date2
	4b		date3
	2b		(only id type 1-files) internal server number,
			  refers to earlier definitions. Is often zero.
	1b		data-length flag
			   0=> No data
			   1=> Next byte is data
			   2=> Next 2 bytes are data
			   3=> Next 3 bytes are data
				...
			   240 => Next 240 bytes are data
			   241=> Next byte is # data bytes to follow
			   242=> Next 2 bytes is # data bytes to follow
			   243=> Next 4 bytes is # data bytes to follow
			   244=> Next 8 bytes is # data bytes to follow
			   The numbers 0..240 are immediatly followed by
			   the data. The number 241 and higher are followed
			   by the number of data bytes(2b) in the current
			   block. Even in the unlikely case the size they
			   specify is zero.
			   That number does not take part in the count.
			   If not all fits into that current block
			   the rest should follow as subtype 2 blocks.
      data bytes definition for the now known id types:
	id type 1 (file):
		   data bytes
	id type 2 (capability) :
		cap	the actual capability
	id type 3 (zombie):
		no data allowed ( data-length flag==0)
	id type 4 (empty capset):
		no data allowed ( data-length flag==0)
	Any unknown type is allowed. Should be announced when not recognized,
	but not force a fatal error.

   Subtype 2 always follows any block in which the data did not fit
   in the first block.
   They take the following form:
	4b	cap # of file
	4b	offset in data, should be consecutive
	2b	# data bytes 
	data bytes

*/

#define	BLOCK_SIZE	1024
#define	LOG2_BLOCK_SIZE	10
#define	HDR_CONSTANT	0x62951413

/* Major and minor types */
#define	AR_HDR		0
#define		AR_IGNORE	0
#define		AR_START	1
#define		AR_EVOL		2
#define		AR_END		3
#define AR_GRAPH	1
#define AR_DIRDEF		0
#define AR_DIRCONT		1
#define AR_DIREND		2
#define AR_CONTENT	2
#define		AR_SERVER	0
#define		AR_DEF		1
#define			AR_EOD			0
#define			AR_FILE			1
#define			AR_CAP			2
#define			AR_NONE			3
#define			AR_EMPTY		4
#define		AR_DATA		2

#define CHK_OFF			12	/* Start of checksum, see w_start() */

/* AR_DEF offset definitions */
#define DEF_BASE	0
#define DEF_L1		241
#define DEF_L2		242
#define DEF_L4		243
#define DEF_L8		244

/* Definition file for buffer administration */

struct ba_list {
	struct ba_list	*next ;		/* The next in the chain */
	int		flags ;		/* Specialities */
	int		rw_pos ;	/* Read/Write position */
	byte		*content ;	/* The buffer */
} ;

/* Possible contents of flags above.. */
#define	BA_END		1		/* Last buffer, no contents */
#define BA_ERROR	2		/* Read error, no content */
#define BA_ABORT	4		/* Abort by producer */

#ifdef __STDC__
void	aio_hdr(struct ba_list *,int type);
void	aw_hdr(int type);
void	aw_gs(void);
void	aw_dir(struct cap_t *p_id, long id);
void	aw_gf(void);
void	aw_file(struct cap_t *p_id, long id);
void	aw_none(struct cap_t *p_id, long id);
void	aw_cap(struct cap_t *p_id, long id);
#else
void	aio_hdr();
void	aw_hdr();
void	aw_gs();
void	aw_dir();
void	aw_gf();
void	aw_file();
void	aw_none();
void	aw_cap();
#endif

#ifdef __STDC__
int		openarch(char *name,int wr_flag, int reopen_flag, int mt_flag);
char		*rio_string(struct ba_list *);
byte		*rio_mem(struct ba_list *,int n);
long		rio_4b(struct ba_list *);
unsigned	rio_2b(struct ba_list *);
int		rio_1b(struct ba_list *);
void		wbl_string(char *s);
void		wbl_cap(capability *cap);
void		wbl_mem(int n, byte *mem);
void		wbl_4b(long val);
void		wbl_2b(unsigned val);
void		wbl_1b(int val);
int		wbl_tell(void);
void		wbl_seek(int pos);
void		w_start(void);
void		w_end(void);
void		wio_string(struct ba_list *,char *s);
void		wio_4b(struct ba_list *,long val);
void		wio_2b(struct ba_list *,unsigned val);
void		wio_1b(struct ba_list *,int val);
void		wio_start(struct ba_list *);
void		wio_end(struct ba_list *);
void		init_write(void);
void		init_read(void);
int		read_block(void);
void		iostats(void);
#else
int		openarch();
char		*rio_string();
byte		*rio_mem();
long		rio_4b();
unsigned	rio_2b();
int		rio_1b();
void		wbl_string();
void		wbl_cap();
void		wbl_mem();
void		wbl_4b();
void		wbl_2b();
void		wbl_1b();
int		wbl_tell();
void		wbl_seek();
void		w_start();
void		w_end();
void		wio_string();
void		wio_4b();
void		wio_2b();
void		wio_1b();
void		wio_start();
void		wio_end();
void		init_write();
void		init_read();
int		read_block();
void		iostats();
#endif

#ifdef MULTI_THREADED
#include <semaphore.h>
#include <module/mutex.h>

struct pool {
	semaphore	count ;		/* # blocks in list */
	mutex		lock ;		/* lock for chain & last */
	mutex		finished ;	/* Unlocked when finished */
	struct ba_list  *first ;	/* the first in the list */
	struct ba_list  *last ;		/* the last in the list */
	int		put_aborted ;	/* No longer useable for put */
	int		get_aborted ;	/* No longer useable for get */
	unsigned long	s_total ;	/* sum of queue sizes */
	long		s_no ;		/* mean = s_total/s_no */
	int		s_max ;		/* Max. Queue length */
};

/* putblock's third variable */
#define AT_BEGIN	0
#define AT_END		1

/* The stack size for new threads */
#define STK_SIZE	2*8192

#ifdef __STDC__
int		put_block(struct pool *,struct ba_list *,int) ;
struct ba_list *get_block(struct pool *) ;
#else
int		put_block();
struct ba_list *get_block() ;
#endif /* STDC */
#endif /* MULTI_THREADED */
