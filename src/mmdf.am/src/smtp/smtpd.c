/*	@(#)smtpd.c	1.4	96/03/05 10:59:32 */
#include "amoeba.h"
#include "stdio.h"
#include "semaphore.h"
#include "circbuf.h"
#include "vc.h"
#include "class/stdstatus.h"
#include "server/process/proc.h"
#include "caplist.h"
#include "am_exerrors.h"
#include "ns.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "thread.h"
#include "smtpsrvr.h"
#include "stderr.h"
#include "monitor.h"
#include "sys/types.h"
#include "sys/wait.h"
#include "module/name.h"
#include "module/rnd.h"
#include "module/prv.h"
#include "module/syscall.h"
#include "module/mutex.h"

#include "server/ip/hton.h"
#include "server/ip/types.h"
#include "server/ip/tcpip.h"
#include "server/ip/tcp_io.h"
#include "server/ip/gen/in.h"
#include "server/ip/gen/tcp.h"
#include "server/ip/gen/tcp_io.h"

/*#define DEBUG*/


char hostname[80];
capability tcpcap;              /* The TCP/IP server */
capability pub_cap;
capability priv_cap;
char *tcpipname;

static mutex mu;

void mon();
void main_loop();
void server_loop();



main(argc, argv)
     int argc;
     char **argv;
{
  errstat err;
  port pubport;
  port check;
  int i = 0;
  char server_cap[128];


  if( argc != 3) {
    exit(1);
  }

  
  sprintf(server_cap,SERVER_CAP,argv[2]);
  strcpy(hostname,argv[2]);

  if (name_lookup(server_cap,&pub_cap) == STD_OK)
    err=name_delete(server_cap);


    MON_EVENT("Creating supercap.");
    uniqport(&priv_cap.cap_port);
    uniqport(&check);
    if (err = prv_encode(&priv_cap.cap_priv,(objnum)SUPER,
	  	 PRV_ALL_RIGHTS, &check) != STD_OK){
      exit(1);
    }   


    priv2pub(&priv_cap.cap_port,&pub_cap.cap_port);
    pub_cap.cap_priv=priv_cap.cap_priv;

    err = name_append(server_cap,&pub_cap);


    if (err != STD_OK){
      exit(1);
    }



  if (!thread_newthread(server_loop,STACKSZ,(char *)0,0)){
    exit(1);
  }

 
  tcpipname = (char *)calloc(strlen(argv[1])+1,sizeof(char));
  strcpy(tcpipname,argv[1]);
			     

     
  mu_init(&mu);


  for (i=0;i<NTHREADS -1;i++){
    if (!thread_newthread(main_loop,STACKSZ,(char *)0,0)){
    exit(1);
    }


  }

  main_loop();

}


errstat impl_std_info(hdr,buf,bufsz,len)
     header *hdr;
     char *buf;
     int bufsz;
     bufsize *len;

{
  sprintf(buf,"SMTP server");
  *len = (bufsize)strlen(buf);
  return(STD_OK);
}


void server_loop()
{
  header hdr;
  char repbuf[BUFSZ];
  bufsize n,repsz;



  for(;;){
    hdr.h_port = priv_cap.cap_port;  /*Use private port for get.*/
    n = getreq(&hdr,(char *)0,0);

    if (ERR_STATUS(n)){
      if (ERR_CONVERT(n) == RPC_ABORTED){
	continue;
      }
      exit(1);
    }

    switch(hdr.h_command){
    case STD_INFO:
      hdr.h_status = (bufsize)impl_std_info(&hdr,repbuf,BUFSZ,&repsz);
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
  port cap1, cap2;          /* Our virtual circuit capabilities */
  capability Cap1,Cap2;
  port prv1,prv2;
  char *args[1+SMTPARGS];
  capability process;
  int i;
  errstat err;
  char template1[128];
  char template2[128];
  int curpid = -1;
  int status = 0;
  int counter = 0;
  int retpid = -1;
  capability chancap;
  nwio_tcpconf_t tcpconf;
  nwio_tcpcl_t tcplistenopt;
#ifdef DEBUG
  unsigned long t_base;
#endif

  for(;;){
/************************************************************************/
#ifdef DEBUG
    t_base= sys_milli();
#endif
    err=name_lookup(tcpipname, &tcpcap);
#ifdef DEBUG
    printf("name_lookup time is: %d\n",sys_milli()-t_base);
#endif


    if (err != STD_OK)
    {
      mon("smtpd could not do tcpip name_lookup", err_why(err));
      sleep(1);
      continue;
    }
/************************************************************************/
    MON_EVENT("attempting tcpip_open");
#ifdef DEBUG
    t_base= sys_milli();
#endif
    err= tcpip_open(&tcpcap, &chancap);
#ifdef DEBUG
    printf("tcpip_open time is: %d\n",sys_milli()-t_base);
#endif


    if (err != STD_OK)
    {
      mon("smtpd could not do tcpip_open", err_why(err));
      sleep(1);
      continue;
    }
    tcpconf.nwtc_flags= NWTC_LP_SET | NWTC_UNSET_RA | NWTC_UNSET_RP;
    tcpconf.nwtc_locport= htons(SMTP_PORT);

/************************************************************************/

#ifdef DEBUG
    t_base= sys_milli();
#endif
    err= tcp_ioc_setconf(&chancap, &tcpconf);
#ifdef DEBUG
    printf("tcp_ioc_setconf time is: %d\n",sys_milli()-t_base);
#endif

    if (err != STD_OK)
    {
      std_destroy(&chancap);
      mon("smtpd could not do tcp_ioc_setconf", err_why(err));
      MON_EVENT("smtpd could not do tcp_ioc_setconf");
      sleep(1);
      continue;
    }
    MON_EVENT("flag 1");
    tcplistenopt.nwtcl_flags= 0;

/************************************************************************/

#ifdef DEBUG
    t_base= sys_milli();
#endif
    err= tcp_ioc_listen(&chancap, &tcplistenopt);
#ifdef DEBUG
    printf("tcp_ioc_listen time is: %d\n",sys_milli()-t_base);
#endif

    if (err != STD_OK)
    {
      std_destroy(&chancap);
      mon("smtpd could not do tcp_ioc_listen", err_why(err));
      MON_EVENT("smtpd could not do tcp_ioc_listen");
      sleep(1);
      continue;
    }

/************************************************************************/

    MON_EVENT("flag 2");

#ifdef DEBUG
    t_base= sys_milli();
#endif
    err= tcpip_keepalive_cap(&chancap);
#ifdef DEBUG
    printf("tcpip_keepalive_cap time is: %d\n",sys_milli()-t_base);
#endif

    if (err != STD_OK)
    {
      std_destroy(&chancap);
      mon("smtpd could not do tcpip_keepalive_cap", err_why(err));
      MON_EVENT("smtpd could not do tcpip_keepalive_cap");
      sleep(1);
      continue;
    }

/************************************************************************/

    MON_EVENT("flag 3");
    strcpy(template1,CAP1);
    mktemp(template1);
#ifdef DEBUG
    t_base= sys_milli();
#endif
    err = name_append(template1,&chancap);
#ifdef DEBUG
    printf("name_append time is: %d\n",sys_milli()-t_base);
#endif

    if (err != STD_OK){
      std_destroy(&chancap);
      mon("Can't publish chancap",err_why(err));
      sleep(1);
      continue;
    }

/************************************************************************/

    MON_EVENT("flag 4");
    args[0] = (char *)calloc(strlen(SMTPSRVR),sizeof(char));
    strcpy(args[0],SMTPSRVR);
    args[1] = (char *)calloc(strlen(template1),sizeof(char));
    strcpy(args[1],template1);
    args[2] = (char *)calloc(strlen(hostname),sizeof(char));
    strcpy(args[2],hostname);
    args[3]=(char *)0;

/************************************************************************/

    if (counter < PERTHREAD){

#ifdef DEBUG
      t_base= sys_milli();
#endif
      curpid = newproc(args[0],args,(char **)0,-1,(int *)0,-1L);      
#ifdef DEBUG
      printf("newproc time is: %d\n",sys_milli()-t_base);
#endif

      if (curpid < 0){
	MON_EVENT("Can't start smtpsrvr process.");
	name_delete(template1);
        sleep(1);
	continue;
      }
      counter++;
      MON_EVENT("Number of thread processes incremented");
    }

/************************************************************************/

    MON_EVENT("flag 5");

    mu_lock(&mu);

#ifdef DEBUG
    t_base= sys_milli();
#endif
    retpid = waitpid(-1,&status,(counter < PERTHREAD) ? WNOHANG : 0);
#ifdef DEBUG
    printf("waitpid time is: %d\n",sys_milli()-t_base);
#endif


    mu_unlock(&mu);
    if (retpid > 0){
      counter--;
      MON_EVENT("Number of thread processes decremented");
    }

/*************************************************************************/

    MON_EVENT("flag 6");

#ifdef DEBUG
    t_base= sys_milli();
#endif
    err= tcpip_keepalive_cap(&chancap);
#ifdef DEBUG
    printf("tcpip_keepalive_cap time is: %d\n",sys_milli()-t_base);
#endif
    

    if (err != STD_OK)
    {
      std_destroy(&chancap);
      mon("smtpd could not do tcpip_keepalive_cap", err_why(err));
      MON_EVENT("smtpd could not do tcpip_keepalive_cap");
    }

/*************************************************************************/

  }

}

void mon(descrip,reason)
     char *descrip;
     char *reason;
{
  char buf[512];

  sprintf(buf,"%s: %s",descrip,reason);
  MON_EVENT(buf);
  return;
}
