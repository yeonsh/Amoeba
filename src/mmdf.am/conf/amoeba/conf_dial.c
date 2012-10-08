/*	@(#)conf_dial.c	1.1	91/11/20 16:49:36 */
#include "util.h"
#include "ll_log.h"
#include "d_proto.h"
#include "d_structs.h"

/* **********************  DIALING TAILOR INFO  **********************  */

/*  modem speed support definitions  */

# define  BELL103        0177     /*  bell 103  0 to 300 baud  */
# define  VA825          0177     /*  vadic 825  0 to 300 baud  */
# define  VA3415         0400     /*  vadic 3415  1200 baud  */

/*
 *     structure defining the available dial-out ports.
 */

#define NUMPRTS         20
LOCVAR struct dialports  theprts[NUMPRTS] =
  {
  0,             0,                0,           0,    0, 0,      0,   0
  };
struct dialports  *d_prts = theprts;
int d_maxprts = NUMPRTS;
int d_numprts = 0;

#define NUMLINES        10
LOCVAR struct directlines  thelines[NUMLINES] =
  {
  0,              0,             0,                 0, 0
  };

int d_maxlines = NUMLINES;
int d_numlines = 0;
struct directlines  *d_lines = thelines;


/*
 *     this is the path name of the file on which outgoing calls are
 *     logged.  it should be owned by the same user as the outgoing
 *     ports and autodialer devices.
 */

char  *d_calllog  =  "/usr/mmdf/log/dial_log";

/*
 *	this is the name of the programs will actually do dialing
 *	it is called as "dial <line> <number>".  It knows which
 *	acus are attached to which lines.
 */

char	*d_dialprog = "/usr/bin/dial";

/* ***************  ILLEGAL CHARACTER TAILORING  ************************* */

/*  The following sets the default illegal-character arrays for both
    dial-out (phone) and dial-in (slave) connections.
 */


unsigned short d_lrill[8] =       /* illegal receive chars              */
{				  /* see d_slvconn & d_masconn          */
    0177777,                      /* nul soh stx etx eot enq ack bel    */
				  /* bs  ht  nl  vt  np  cr  so  si     */
    0177777,                      /* dle dc1 dc2 dc3 dc4 nak syn etb    */
				  /* can em  sub esc fs  gs  rs  us     */
    0000010,                      /* sp  ! " # $ % & ' ( ) * + , - . /  */
    0000000,                      /* 0 1 2 3 4 5 6 7 8 9 : ; < = > ?    */
    0000001,                      /* @ A B C D E F G H I J K L M N O    */
    0010000,                      /* P Q R S T U V W X Y Z [ \\] ^ _    */
    0000000,                      /* ` a b c d e f g h i j k l m n o    */
    0100000                       /* p q r s t u v w x y z { | } ~ del  */
};

unsigned short d_lxill[8] =       /* illegal send chars                 */
{				  /* see d_slvconn & d_masconn          */
    0177777,                      /* nul soh stx etx eot enq ack bel    */
				  /* bs  ht  nl  vt  np  cr  so  si     */
    0177777,                      /* dle dc1 dc2 dc3 dc4 nak syn etb    */
				  /* can em  sub esc fs  gs  rs  us     */
    0000010,                      /* sp  ! " # $ % & ' ( ) * + , - . /  */
    0000000,                      /* 0 1 2 3 4 5 6 7 8 9 : ; < = > ?    */
    0000001,                      /* @ A B C D E F G H I J K L M N O    */
    0010000,                      /* P Q R S T U V W X Y Z [ \\] ^ _    */
    0000000,                      /* ` a b c d e f g h i j k l m n o    */
    0100000                       /* p q r s t u v w x y z { | } ~ del  */
};

/************************** PHONE LOGGING **************************/

struct ll_struct    ph_log =       /* dial-out link-level package log    */
{
    "ph.log", "PH-0000", LLOGFST, 25, LLOGSOME, 0
};

char *def_trn = "ph.trn";          /* dial-out link-level i/o transcript */
				  /* does not use ll_log package        */
