/*	@(#)conf.h	1.1	91/11/20 16:49:27 */
#
/*          This contains the most site-dependent definitions             */

#define USERSIZE   64		/* max length of a user system name   */
#define	MAILIDSIZ  64		/* max length of a mailid,
				 * must be at least USERSIZE.		*/
#define MSGNSIZE   14           /* max length of a message file name  */
#define FILNSIZE  128           /* max length of full file name       */
#define ADDRSIZE  256		/* max length of an address           */
#define LINESIZE  256		/* max length of a line/record        */

#define	NUMCHANS   20		/* max number of channels */
#define NUMTABLES  30		/* max number of tables */
#define NUMDOMAINS 20		/* max number of domains */

/************  DIAL IN/OUT (CH_PHONE / SLAVE) TAILORING  *****************/

#define DL_TRIES    2             /* number of re-connects OK for phone */
#define DL_RCVSZ    255           /* longest line receivable            */
#define DL_XMTSZ    255           /* longest line sendable              */

/****************  JNTMAIL Tailoring *******************/

#undef	JNTMAIL
#undef	BOTHEND
#undef	VIATRACE

/**************** Include File Tailoring ****************************/
#ifdef AMOEBA
#include "time.h"
#else
#include <time.h>
#endif

#define S5DIRENT           /*Use <dirent.h> && struct dirent */
/*define BSDDIRECT         Use <sys/dir.h> && struct direct */


#ifdef S5DIRENT
#include "dirent.h"
#define dirtype dirent
#else
#include "sys/dir.h"
#define dirtype direct
#endif






/********************* Privileged UIDs *************************/

/*
 *  This can be a macro as give here or you can provide a function.
 *  It should always authorize root and mmdf.
 *  The global variable effecid is assumed to have the MMDF uid in it.
 */

#define	PRIV_USER(x) ((x)==0 || (x)==1 || (x)==effecid)





