/*	@(#)boot.c	1.7	96/03/07 14:08:53 */
/*
 * Copyright 1996 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "caplist.h"
#include "monitor.h"
#include "svr.h"
#include "module/proc.h"
#include "module/tod.h"
#include "module/rnd.h"
#include "string.h"

#define REPORT_ERR(bp)	if (debugging || verbose) \
				prf("%n%s: %s\n", bp->or.or_data.name, bp->or.or_errstr)

/*
 *	Spawn a process. Assumes *cmd is locked
 */
int
spawn(boot, cmd)
    obj_rep *boot;
    boot_cmd *cmd;
{
    capability execcap, machcap, hostcap;
    errstat err;
    struct caplist capenv[MAX_CAPENV + 1];
    capability envcaps[MAX_CAPENV];
    static mutex serialise;	/* Don't trash the bullet server */
    capability ownercap;

    /* Find executable, machine */
    err = ref_lookup(&boot->or.or_data.machine, &machcap, (obj_rep *) NULL);
    if (err != STD_OK) {
	sprintf(boot->or.or_errstr, "cannot lookup machine \"%s\" (%s)",
				    boot->or.or_data.machine.path, err_why(err));
	REPORT_ERR(boot);
	return 0;
    }
    if (boot->or.or_data.flags & BOOT_ONPROCSVR) hostcap = machcap;
    else {
	err = dir_lookup(&machcap, PROCESS_SVR_NAME, &hostcap);
	if (err != STD_OK) {
	    sprintf(boot->or.or_errstr, "cannot lookup \"%s/%s\" (%s)",
		    boot->or.or_data.machine.path, PROCESS_SVR_NAME, err_why(err));
	    REPORT_ERR(boot);
	    return 0;
	}
    }
    err = ref_lookup(&cmd->executable, &execcap, (obj_rep *) NULL);
    if (err != STD_OK) {
	sprintf(boot->or.or_errstr, "cannot lookup executable \"%s\" (%s)",
				cmd->executable.path, err_why(err));
	REPORT_ERR(boot);
	return 0;
    }

    /* Create capability environment */
    {
	int i;
	for (i = 0; i < cmd->ncaps; ++i) {
	    capenv[i].cl_name = cmd->capv[i].cap_name;
	    if (cmd->capv[i].from_svr) {	/* Inherit from me */
		/* The I/O capabilities are special: */
		if (strcmp(cmd->capv[i].cap_name, "STDIN") == 0 ||
		    strcmp(cmd->capv[i].cap_name, "STDOUT") == 0 ||
		    strcmp(cmd->capv[i].cap_name, "STDERR") == 0) {
			extern capability *outcap;
			capenv[i].cl_cap = outcap;
		} else if ((capenv[i].cl_cap = getcap(cmd->capv[i].cap_name)) == NULL) {
		    sprintf(boot->or.or_errstr, "cannot inherit env-cap \"%s\"",
					cmd->capv[i].cap_name);
		    REPORT_ERR(boot);
		    return 0;
		}
	    } else {
		err = ref_lookup(&cmd->capv[i].cap_ref, &envcaps[i],
							(obj_rep *) NULL);
		if (err != STD_OK) {
		    sprintf(boot->or.or_errstr, "cannot lookup env-cap \"%s\" (%s)",
					cmd->capv[i].cap_name, err_why(err));
		    REPORT_ERR(boot);
		    return 0;
		}
		capenv[i].cl_cap = &envcaps[i];
	    }
	}
	capenv[cmd->ncaps].cl_name = NULL;
	capenv[cmd->ncaps].cl_cap = NULL;
    }
    { /* This is a new process, so we need a new random port */
	port random;
	uniqport(&random);
	prv_encode(&boot->or.or_objcap.cap_priv, prv_number(&boot->or.or_objcap.cap_priv),
					PRV_ALL_RIGHTS, &random);
	boot->or.or_objcap.cap_port = putcap.cap_port;
	SaveState = 1;	/* Remember we need to save state */
    }
    ownercap = boot->or.or_objcap;

    mu_lock(&serialise);	/* Don't saturate the bullet server */
    /* Execute the program */
    err = exec_file(
	&execcap,		/* prog */
	&hostcap,		/* host */
	&ownercap,		/* owner */
	cmd->stack,		/* stacksize */
	cmd->argv,		/* command and arguments */
	cmd->envp,		/* envp */
	cmd->ncaps ? capenv : NULL, /* cap env */
	&boot->or.or_proccap		/* process return */
    );
    MU_CHECK(serialise);
    mu_unlock(&serialise);

    if (err != STD_OK) {
	MON_EVENT("exec_file() != STD_OK");
	sprintf(boot->or.or_errstr, "exec_file failed (%s)", err_why(err));
	REPORT_ERR(boot);
	if (err == STD_CAPBAD && (debugging || verbose)) {
	    char ans[80];
	    int len;

	    /*
	     *	CAPBAD can refer to either the host or
	     *	the executable....
	     *	Make a neat error message by inquiring
	     *	both host and executable:
	     */
	    err = std_info(&execcap, ans, (int) (sizeof(ans) - 1), &len);
	    if (err == STD_OVERFLOW) { /* ignore the rest */
		len = sizeof(ans) - 1;
		err = STD_OK;
	    }
	    if (err == STD_OK) {
		ans[len] = '\0';
		prf("%nStdInfo prog %s: '%s'\n", ar_cap(&execcap), ans);
	    } else prf("%nCannot stdinfo prog %s (%s)\n",
			ar_cap(&execcap), err_why(err));

	    err = std_info(&hostcap, ans, (int) (sizeof(ans) - 1), &len);
	    if (err == STD_OVERFLOW) { /* ignore the rest */
		len = sizeof(ans) - 1;
		err = STD_OK;
	    }
	    if (err == STD_OK) {
		ans[len] = '\0';
		prf("%nStdInfo host %s: '%s'\n", ar_cap(&hostcap), ans);
	    } else prf("%nCannot stdinfo host %s (%s)\n",
			ar_cap(&hostcap), err_why(err)); }
    } else {	/* Exec worked; set comment string, remember process cap */
	char comment[80], *p, **arg;
	int dummy;
	(void) strcpy(comment, "Daemon:");
	p = comment + strlen(comment);
	arg = cmd->argv;
	while (arg != NULL && *arg != NULL &&
	  p + strlen(*arg) + 2 < comment + sizeof(comment)) {
	    *p++ = ' ';
	    (void) strcpy(p, *arg++);
	    p += strlen(p);
	}
	(void) pro_setcomment(&boot->or.or_proccap, comment);
	SaveState = 1;	/* Remember we got a new process cap */
	boot->or.or_status &= ~ST_ISIDLE;
	/* We cannot handle this error: */
	(void) tod_gettime(&boot->or.or_boottime, &dummy, &dummy, &dummy);
    }

    return err == STD_OK;
} /* spawn */

/*
 *	Start a service
 */
void
do_boot(boot)
    obj_rep *boot;
{
    Time_t now;
    capability proccap;

    if (boot->or.or_data.flags & BOOT_INACTIVE) {
	/* should not be booted at all */
	return;
    }

    now = my_gettime();
    if (later(boot->or.or_nextboot, now)) {
	if (debugging)
	    prf("%nNot yet time to reboot %s; %d msecs to go\n",
		boot->or.or_data.name, boot->or.or_nextboot - now);
	return;
    } else if (debugging) prf("%n%d msecs late\n", now - boot->or.or_nextboot);
    boot->or.or_nextboot = add_time(now, boot->or.or_data.bootrate);
    boot->or.or_retries = 0;
    if (debugging || verbose) prf("%nBoot(%s)\n", boot->or.or_data.name);

    proccap = boot->or.or_proccap;
    if (!spawn(boot, &boot->or.or_data.bootcmd)) return;
    if (boot->or.or_status & ST_GOTPROC) {
	errstat err;
	if (debugging) prf("%nbefore stun\n");
	err = pro_stun(&proccap, -9L);
	/*
	 * The checkpoint will not be received
	 * properly: the random word has changed
	 * to reflect the new process cap, but
	 * who cares? We want to get rid of it.
	 */
	if (verbose || debugging)
	    prf("%npro_stun old incarnation of %s: %s\n",
		boot->or.or_data.name, err_why(err)); 
    } else boot->or.or_status |= ST_GOTPROC;
    MON_EVENT("Spawned process");

    /* Set it again - we may have been waiting */
    boot->or.or_nextboot = add_time(my_gettime(), boot->or.or_data.bootrate);
    if (debugging) prf("%ndo_boot returns\n");
} /* do_boot */
