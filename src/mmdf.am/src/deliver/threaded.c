/*	@(#)threaded.c	1.5	96/02/27 09:58:15 */
/*
 *     MULTI-CHANNEL MEMO DISTRIBUTION FACILITY  (MMDF)
 *
 *
 *     Copyright (C) 1979,1980,1981  University of Delaware
 *
 *     Department of Electrical Engineering
 *     University of Delaware
 *     Newark, Delaware  19711
 *
 *     Phone:  (302) 738-1163
 *
 *     This program module was developed as part of the University
 *     of Delaware's Multi-Channel Memo Distribution Facility (MMDF).
 *
 *     This module and its listings may be used freely by United States
 *     federal, state, and local government agencies and by not-for-
 *     profit institutions, after sending written notification to
 *     Professor David J. Farber at the above address.
 *
 *     For-profit institutions may use this module, by arrangement.
 *
 *     Notification must include the name of the acquiring organization,
 *     name and contact information for the person responsible for
 *     maintaining the operating system, name and contact information
 *     for the person responsible for maintaining this program, if other
 *     than the operating system maintainer, and license information if
 *     the program is to be run on a Western Electric Unix(TM) operating
 *     system.
 *     
 *     Documents describing systems using this module must cite its
 *     source.
 *     
 *     Development of this module was supported in part by the
 *     University of Delaware and in part by contracts from the United
 *     States Department of the Army Readiness and Materiel Command and
 *     the General Systems Division of International Business Machines.
 *     
 *     Portions of the MMDF system were built on software originally
 *     developed by The Rand Corporation, under the sponsorship of the
 *     Information Processing Techniques Office of the Defense Advanced
 *     Research Projects Agency.
 *
 *     The above statements must be retained with all copies of this
 *     program and may not be removed without the consent of the
 *     University of Delaware.
 *     
 *
 *     version  -1    David H. Crocker    March  1979
 *     version   0    David H. Crocker    April  1980
 *     version   1    David H. Crocker    May    1981
 *
 */
#include "util.h"
#include "mmdf.h"
#include "signal.h"
#include "sys/stat.h"
#include "pwd.h"
#include "sgtty.h"
#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "deliversvr.h"
#include "stderr.h"
#include "thread.h"
#include "monitor.h"
#include "module/proc.h"
#include "module/name.h"
#include "module/prv.h"
#include "module/rnd.h"
#include "module/stdcmd.h"
#include "module/mutex.h"

/* ***        RUN SETUID TO ROOT, so it can setuid to MMDF          *** */
/* ***                                                              *** */
/* ***  This is so that BOTH real and effective id's show as MMDF   *** */

/*
 *  OVERVIEW
 *
 *      The program may be called to process the entire mail queue or just
 *  handle some explicitly-named messages.  It will try all available
 *  channels that the mail needs to go out on, or will limit itself to
 *  explicitly, named channels.  It tries to minimize channel invocations,
 *  and process messages in order.
 *
 *      A list of channels to process is made from an argument, or simply
 *  is copied from the list of all active channels.
 *
 *      A list of messages to process is made from the arguments, or is
 *  built from a scan of the entire queue.  This list is in-core, so it is
 *  subject to serious size limits.  If more than 1000 messages are in the
 *  queue, the list is not built and the message names are read directly
 *  from the directory, as the messages are to be processed.
 *
 *      With an incore list, the messages can be sorted, according to
 *  creation date, and therefore processed in order.  In the overflow
 *  condition, this is not possible.  The primary sorting key is channels,
 *  with messages next.  This causes many passes over the messages (i.e.,
 *  access to their address lists) but minimizes channel invocations.  A
 *  final sorting key should be hosts on the channels, to minimize re-
 *  connections.  This is done if mod=host is set and deliver is working from
 *  an in-core queue.
 */
/**/

/*  DELIVER:  Mail transmission manager
 *
 *  May 80 Dave Crocker     fix fd, sig, setuid handling & extral dial
 *  Jun 80 Dave Crocker     mn_mmdf reset userid, when setuid to mmdf
 *                          fix fd bugs in rtn_report
 *                          added ovr_free, fixing abnormal ending
 *  Aug 80 Dave Crocker     moved ovr_free, so free always done
 *                          removed use of ovr_seq
 *                          created msg_note, to control printout
 *  Sep 80 Dave Crocker     added -r switch to rtn-report mail call
 *  Oct 80 Dave Crocker     converted to stdio & V7 ability
 *                          split off rtn_io and ml_io
 *  Nov 80 Dave Crocker     re-organized ovr_ module
 *  Jun 81 Dave Crocker     remove use of ffio package
 *                          change adr_t2ok -> adr_t2fin, to allow
 *                          permanent failure during text phase
 *                          move adr_tok=FALSE from adr_t2fin to adr_done
 *  Jul 81 Dave Crocker     ovr_mfree check for valid ovr_mlist before free
 *         Mike Minnich     add -s, to force no-sort of queue
 *         Dave Crocker     ovr_cstep had bad fread return test
 *                          upped max # messages to 1000
 *                          adr_more=TRUE in adr_done() on temp error
 *                          disable SIGQUIT, for background mode
 *  Aug 81 Dave Crocker     change mn_*init ordering for safe callerid refs
 *                          allow partitioning queue by message size
 *  Sep 81 Dave Crocker     adr_send() added RP_RPLY and RP_PROT to BHST
 *                          case for 'temporary' error
 *  Nov 81 Dave Crocker     ovr_ismsg require message name begin "msg."
 *  Dec 81 Dave Crocker     ovr_curch -> arg rather than global
 *                          ch_init called with chan ptr, not char code
 *  Jan 82 Dave Crocker     add setgid, along with setuid
 *  Mar 83 Doug Kingston    changed to use format independent directory
 *                          access routines a la 4.2BSD  (libndir.a)
 *  1991   Lisa Bahler	    made it work under amoeba
 */

extern LLog msglog;
extern char *logdfldir;
LLog *logptr = &msglog;
extern char *cmddfldir;

static mutex mu;
capability pub_cap;
capability inf_cap;
capability mmdf_cap;
capability del_cap;
port infoport;
port delport;
errstat err;
errstat impl_std_info();
errstat impl_deliver();
void server_loop();
void main_loop();
errstat start_background();

main ()
{
  port pubport;
  port check;
  int j = 0;
  char tempbuf[256];
  char infobuf[BUFSZ];
  int len;

  err = name_lookup(BACKGRND_CAP,&pub_cap);
  if (err == STD_OK){
    err = std_info(&pub_cap,infobuf,BUFSZ,&len);
    if (err != STD_OK){
      MON_EVENT("Bckgrnd deliver not found -- starting background deliver.");
      name_delete(BACKGRND_CAP);
      err = start_background();
    }
    else
      MON_EVENT("Background deliver already exists--not creating new one.");
  }
  else if (err == STD_NOTFOUND){
    MON_EVENT("No background deliver dir entry--starting background deliver.");
    err = start_background();
  }

  if (err != STD_OK){
    MON_EVENT("Could not create background deliver!");
  }

  if (name_lookup(SERVER_CAP,&pub_cap) == STD_OK)
    err = name_delete(SERVER_CAP);

  uniqport(&inf_cap.cap_port);
  uniqport(&check);
  if (prv_encode(&inf_cap.cap_priv,(objnum)SUPER,
					       PRV_ALL_RIGHTS, &check) != 0){
    exit(1);
  }
  priv2pub(&inf_cap.cap_port,&pub_cap.cap_port);
  pub_cap.cap_priv = inf_cap.cap_priv;

  err = name_append(SERVER_CAP,&pub_cap);

  if (err != STD_OK){
    exit(1);
  }

  sprintf(tempbuf,"chm ff:ff:ff %s",SERVER_CAP);
  system(tempbuf);


    if (name_lookup(MMDF_CAP,&mmdf_cap) == STD_OK)
      err = name_delete(MMDF_CAP);


    uniqport(&del_cap.cap_port);
    uniqport(&check);
    if (prv_encode(&del_cap.cap_priv,(objnum)SUPER,
		   PRV_ALL_RIGHTS, &check) != 0){
      exit(1);
    }
    priv2pub(&del_cap.cap_port,&mmdf_cap.cap_port);
    mmdf_cap.cap_priv = del_cap.cap_priv;

    err = name_append(MMDF_CAP,&mmdf_cap);

  if (err != STD_OK){
    exit(1);
  }

  sprintf(tempbuf,"chm ff:ff:ff %s",MMDF_CAP);
  system(tempbuf);


  mu_init(&mu);

  if(!thread_newthread(server_loop,STACKSZ,(char *)0,0)){
    exit(1);
  }

  for(j=0;j<NTHREADS-1;j++)
    if(!thread_newthread(main_loop,STACKSZ,(char*)0,0)){
      MON_EVENT("Cannot start main thread.");
    }
  main_loop();

}


void server_loop()
{
  header hdr;
  char reqbuf[BUFSZ];
  char repbuf[BUFSZ];
  bufsize n,repsz;

  for(;;){
    hdr.h_port = inf_cap.cap_port;
    n = getreq(&hdr,reqbuf,BUFSZ);

    if (ERR_STATUS(n)){
      if (ERR_CONVERT(n) == RPC_ABORTED){
	continue;
      }
      MON_EVENT("deliversvr getreq error");
      exit(1);
    }

    switch(hdr.h_command){
    case STD_INFO:
      hdr.h_status = (bufsize)impl_std_info(&hdr,repbuf,BUFSZ,&repsz,0);
      hdr.h_size = repsz;
      break;
    default:
      hdr.h_status = STD_COMBAD;
      break;
    }

    putrep(&hdr,repbuf,repsz);
  }
}

void main_loop()
{
  header hdr;
  char reqbuf[BUFSZ];
  char repbuf[BUFSZ];
  bufsize n,repsz;

  for(;;){
    hdr.h_port = del_cap.cap_port; 
    n = getreq(&hdr,reqbuf,BUFSZ);

    if (ERR_STATUS(n)){
      if (ERR_CONVERT(n) == RPC_ABORTED){
	continue;
      }
      MON_EVENT("deliversvr getreq error");
      exit(1);
    }

    switch(hdr.h_command){
    case MMDF_DELIV:
      hdr.h_status = (bufsize)impl_deliver(&hdr,reqbuf);
      repsz = hdr.h_size = 0;
      break;
    case STD_INFO:
      hdr.h_status = (bufsize)impl_std_info(&hdr,repbuf,BUFSZ,&repsz,1);
      hdr.h_size = repsz;
      break;
    default:
      hdr.h_status = STD_COMBAD;
      break;
    }

    putrep(&hdr,repbuf,repsz);
  }
}

errstat impl_std_info(hdr,buf,bufsz,len,msgtype)
     header *hdr;
     char *buf;
     int bufsz;
     bufsize *len;
     int msgtype;
{
  MON_EVENT("Doing std_info");
  switch (msgtype) {
  case 0:
    sprintf(buf,"Deliver Server");
    break;
  case 1:
    sprintf(buf,"MMDF msg handler");
    break;
  default:
    return(STD_ARGBAD);
    break;
  }

  *len = (bufsize)strlen(buf);
  return(STD_OK);
}

extern char *strdup();

errstat impl_deliver(hdr,buf)
     header *hdr;
     char *buf;
{
  int i;
  int argc = 0;
  char **argv;
  char deliverguts[256];
  errstat err = 0;
  capability process;
  char *temp;
  int curpid = -1;
  int status = 0;
  int counter = 0;
  int retpid = -1;

  MON_EVENT(buf);
  argc = atoi(strtok(buf,"\n"));
  argv = (char **)calloc(argc+2,sizeof(char *));

  sprintf(deliverguts,"%s/deliverguts",cmddfldir);
  if ((argv[0] = strdup(deliverguts)) == NULL) {
	return STD_NOSPACE;
  }

  argv[argc + 1] = (char *)0;

  for (i=1;i<=argc;i++){
    temp = (char *)strtok((char *)0,"\n");
    if ((argv[i] = strdup(temp)) == NULL) {
	return STD_NOSPACE;
    }
  }

  MON_EVENT("about to start deliverguts");
  curpid = newproc(argv[0],argv,(char**)0,-1,(int*)0,-1L);


  for (i=0;i<=argc;i++)
    free(argv[i]);

  free(argv);


  if (curpid < 0){
    MON_EVENT("Can't start deliverguts process.");
    return(STD_OK);
  }
  else
    MON_EVENT("Number of thread processes incremented.");


  mu_lock(&mu);
  retpid = waitpid(/*curpid*/-1,&status,0);
  mu_unlock(&mu);

  if (retpid > 0)
    MON_EVENT("Number of thread processes decremented.");


  return(STD_OK);
}

errstat start_background()
{
  char *args[4];
  char background[256];
  capability process;


    sprintf(background,"%s/deliverguts",cmddfldir);
    args[0] = strdup(background);
    args[1] = strdup("deliver");
    args[2] = strdup("-b");
    if ( !args[0] || !args[1] || !args[2] ) {
	return STD_NOSPACE;
    }

    /* args[3] = strdup("-o1"); */
    args[3] = (char *)0;

    err = exec_file((capability *)0, (capability *)0, (capability *)0, 0,
		    args, (char **)0, (struct caplist *)0, &process);
    if (err != STD_OK) {
	return err;
    }

    err = name_append(BACKGRND_CAP,&process);

    return(err);

}

