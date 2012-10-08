/*	@(#)vdflip.c	1.1	96/02/27 11:49:56 */
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#define ffs     string_ffs
#include "sys/flip/flip.h"
#include "sys/flip/packet.h"
#include "sys/flip/interface.h"
#include "assert.h"
#include "sys/debug.h"
#undef ffs
#undef timeout
#include "ux_int.h"
#include "ux_rpc_int.h"

#define threadp	unix_threadp
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/modctl.h>
#include <sys/devops.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#undef threadp

#include "fldefs.h"

#define VERSION		"FLIP driver v1.00"

kmutex_t flopen_mutex;
kmutex_t sleep_mutex;
kmutex_t flip_mutex;

static int flgetinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int flidentify(dev_info_t *);
static int flattach(dev_info_t *, ddi_attach_cmd_t);
static int fldetach(dev_info_t *, ddi_detach_cmd_t);

dev_info_t saved_dip;


static struct module_info flmodinfo = {
	0x0033,		/* module id number (not officially reserved) */
	"flip",		/* module name */
	0,		/* min stream message size accepted */
	INFPSZ,		/* max stream message size accepted */
	65535,		/* hi-water mark */
	1		/* lo-water mark */
};


/* upper read init structure */
static struct qinit urinit = {
	NULL,		/* upper read put procedure */
	NULL,		/* upper read service procedure */
	flopen,		/* called on startup */
	flclose,	/* called on finish */
	NULL,		/* for future use */
	&flmodinfo,	/* module information structure */
	NULL		/* module statistics structure */
};


/* upper write init structure */
static struct qinit uwinit = {
	fluwput,	/* upper write put procedure */
	fluwsrv,	/* upper write service procedure */
	NULL,		/* called on startup */
	NULL,		/* called on finish */
	NULL,		/* for future use */
	&flmodinfo,	/* module information structure */
	NULL		/* module statistics structure */
};


/* lower read init structure */
static struct qinit lrinit = {
	fllrput,	/* lower read put procedure */
	fllrsrv,	/* lower read service procedure */
	NULL,		/* called on startup */
	NULL,		/* called on finish */
	NULL,		/* for future use */
	&flmodinfo,	/* module information structure */
	NULL		/* module statistics structure */
};


/* lower write init structure */
static struct qinit lwinit = {
	NULL,		/* lower write put procedure */
	NULL,		/* lower write service procedure */
	NULL,		/* called on startup */
	NULL,		/* called on finish */
	NULL,		/* for future use */
	&flmodinfo,	/* module information structure */
	NULL		/* module statistics structure */
};


struct streamtab fltab = {
	&urinit,	/* upper read QUEUE */
	&uwinit,	/* upper write QUEUE */
	&lrinit,	/* lower read QUEUE */
	&lwinit		/* lower write QUEUE */
};


static struct cb_ops cb_fl_ops = {
	nulldev,		/* cb_open */
	nulldev,		/* cb_close */
	nodev,			/* cb_strategy */
	nodev,			/* cb_print */
	nodev,			/* cb_dump */
	nodev,			/* cb_read */
	nodev,			/* cb_write */
	nodev,			/* cb_ioctl */
	nodev,			/* cb_devmap */
	nodev,			/* cb_mmap */
	nodev,			/* cb_segmap */
	nochpoll,		/* cb_chpoll */
	ddi_prop_op,		/* cb_prop_op */
	&fltab,			/* cb_stream */
	D_MP | D_NEW 		/* cb_flag */
};


static struct dev_ops fl_ops = {
	DEVO_REV,			/* devo_rev */
	0,				/* devo_refcnt */
	flgetinfo,			/* devo_getinfo */
	flidentify,			/* devo_identify */
	nulldev,			/* devo_probe */
	flattach,			/* devo_attach */
	fldetach,			/* devo_detach */
	nodev,				/* devo_reset */
	&cb_fl_ops,			/* devo_cb_ops */
	(struct bus_ops *) NULL,	/* devo_bus_ops */
	NULL				/* devo_power */
};

static struct modldrv modldrv = {
	&mod_driverops,			/* drv_modops */
	VERSION,			/* drv_linkinfo */
	&fl_ops				/* drv_dev_ops */
};


static struct modlinkage modlinkage = {
	MODREV_1,	/* rev of loadable modules system */
	&modldrv,	/*
	NULL		 * NULL terminated list of linkage structures
			 */
};


int
_init(void)
{
	int	r;

	DPRINTF(1, ("flip: _init\n"));
	r = mod_install(&modlinkage);
	if (r == 0) {
		mutex_init(&flopen_mutex, "flopen_mutex", MUTEX_DRIVER, NULL);
		mutex_init(&sleep_mutex, "sleep_mutex", MUTEX_DRIVER, NULL);
		mutex_init(&flip_mutex, "flip_mutex", MUTEX_DRIVER, NULL);
		unix_init();
	}
	return(r);
}


int
_fini(void)
{
	int r;

	DPRINTF(1, ("flip: _fini\n"));
	r = mod_remove(&modlinkage);
	mutex_destroy(&flopen_mutex);
	mutex_destroy(&sleep_mutex);
	mutex_destroy(&flip_mutex);
	return(r);
}


int
_info(struct modinfo *modinfop)
{
	int r;

	DPRINTF(1, ("flip: _info\n"));
	r = mod_info(&modlinkage, modinfop);
	return(r);
}


static int
flidentify(dev_info_t *devi)
{
        DPRINTF(1, ("flip: flidentify\n"));
        if (strcmp(ddi_get_name(devi), "flip") == 0)
                return(DDI_IDENTIFIED);
        else
                return(DDI_NOT_IDENTIFIED);
}
 

static int
flattach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int i;
	char buf[64];	/* place holder for minor device name */

        DPRINTF(1, ("flip: flattach\n"));
	if (ddi_create_minor_node(dip, "flip_rpc0", S_IFCHR, 0, NULL, CLONE_DEV)
						== DDI_FAILURE) {
		return(DDI_FAILURE);
	}
	if (ddi_create_minor_node(dip, "flip_ctrl", S_IFCHR, FIRST_CTRL,
					DDI_PSEUDO, 0) == DDI_FAILURE) {
		return(DDI_FAILURE);
	}
	ddi_report_dev(dip);
	saved_dip = *dip;
        return(DDI_SUCCESS);
}
 
 

static int
fldetach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
        DPRINTF(1, ("flip: fldetach\n"));
	if (timeoutval != -1)
		untimeout(timeoutval);
        ddi_remove_minor_node(devi, "flip_rpc0");
        ddi_remove_minor_node(devi, "flip_ctrl");
        return(DDI_SUCCESS);
}


static int flgetinfo(
        dev_info_t      *dip,
        ddi_info_cmd_t  infocmd,
        void            *arg,
        void            **result
)
{
        register dev_t  dev = (dev_t) arg;
        register int    instance;
        register int    error;
 
        DPRINTF(1, ("flip: flgetinfo\n"));
	instance = getminor(dev);

        switch (infocmd) {
                case DDI_INFO_DEVT2DEVINFO:
                        if (dip == NULL) {
                                error = DDI_FAILURE;
                        } else {
                                *result = (void *) &saved_dip;
                                error = DDI_SUCCESS;
                        }
                        break;
                case DDI_INFO_DEVT2INSTANCE:
                        *result = (void *) 0;
                        error = DDI_SUCCESS;
                        break;
                default:
                        error = DDI_FAILURE;
        }
        return(error);
}
