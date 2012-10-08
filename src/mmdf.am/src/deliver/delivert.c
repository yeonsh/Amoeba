/*	@(#)delivert.c	1.2	96/02/27 09:58:03 */
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
#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "deliversvr.h"
#include "stderr.h"
#include "module/name.h"

extern char *dupfpath();
extern LLog msglog;
extern char *logdfldir;
LLog *logptr= &msglog;


main (argc, argv)
int       argc;
char   *argv[];
{

  capability svr;
  errstat err;
  header hdr;
  bufsize t = 0;
  int i = 0;
  char argbuffer[BUFSZ];
  int len = 0;
  int count = 0;
char buf[BUFSZ];


  if(logdfldir != (char *)0)
    logptr->ll_file = dupfpath(logptr->ll_file,logdfldir);

  logptr->ll_level=LLOGFTR;

  timeout(0);

  sprintf(argbuffer,"%d\n",argc);
  len = strlen(argbuffer);

  for (i=0;i<argc;i++){
    sprintf(&argbuffer[len],"%s\n",argv[i]);
    len += strlen(argv[i]) +1;
  }

  ll_log(logptr,LLOGFTR,"about to do name lookup of %s",LOOKUP_CAP);

  if ((err = name_lookup(LOOKUP_CAP,&svr)) != STD_OK){
    ll_log(logptr,LLOGFAT,"name lookup of %s failed for reason %s",LOOKUP_CAP,err_why(err));
    exit(2);
  }

  hdr.h_port = svr.cap_port;
  hdr.h_priv = svr.cap_priv;
  hdr.h_command = MMDF_DELIV;


  t = trans(&hdr,argbuffer,len,&hdr,buf, BUFSZ);
  
  if (ERR_STATUS(t)){
    hdr.h_status = t;
    ERR_CONVERT(t);
    ll_log(logptr,LLOGFAT,"trans failed for reason %s",err_why(t));
  }
  else
    ll_log(logptr,LLOGFTR,"trans succeeded");

  if (hdr.h_status != STD_OK) {
    exit(1);
  }

  exit(0);

}











