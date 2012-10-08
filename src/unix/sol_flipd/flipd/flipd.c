/*	@(#)flipd.c	1.1	96/02/27 11:49:44 */
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/conf.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/dlpi.h>
#include <kvm.h>
#include <nlist.h>
#include <stropts.h>
#include <sys/stream.h>

/* #define _KERNEL to get definitions of some kernel structures */
#define _KERNEL
#include <inet/common.h>
#include <inet/ip.h>
#undef _KERNEL

#include <sys/ioccom.h>
#define _IOC_INOUT      IOC_INOUT
#define _IOCPARM_MASK   IOCPARM_MASK

#define FLIPDEVICE	"/dev/flip_rpc0"
#define ETHERTYPE_FLIP	0x8146
#define BUF_SIZE	8192



void
handler(int signal)
{
    printf("FLIP: disconnecting\n");
}


int
main(int argc, char **argv)
{
    int	fdflip;
    int	fdle;
    long buf[BUF_SIZE];
    int	ppa;
    int pid;
    kvm_t *kd;
    struct nlist nl[2];
    unsigned long addr;
    unsigned long buf_addr;
    ill_t ill;
    char if_name[64];

    /* Become a daemon */
    pid = fork();
    if (pid == -1) {
	fprintf(stderr, "%s: fork failed\n", argv[0]);
	exit(1);
    }
    if (pid != 0) {
	exit(0);
    } else {
	setsid();
    }

    /* Open the FLIP device driver */
    if ((fdflip = open(FLIPDEVICE, O_RDWR)) == -1) {
	perror(FLIPDEVICE);
	exit(1);
    }

    /* Get a kvm kernel descriptor */
    if ((kd = kvm_open(NULL, NULL, NULL, O_RDONLY, argv[0])) == NULL) {
	perror("kvm_open");
	exit(1);
    }

    /* Create a name list to lookup the kernel variable "ill_g_head" */
    nl[0].n_name = "ill_g_head";
    nl[1].n_name = "";

    /* Get information of kernel variable "ill_g_head" */
    if (kvm_nlist(kd, nl) == -1) {
	perror("kvm_nlist");
	exit(1);
    }

    /* "ill_g_head" is a pointer to a pointer to a ill_t */
    addr = nl[0].n_value;
    if (kvm_read(kd, addr, (char *) &buf_addr, sizeof(buf_addr)) == -1) {
	perror("kvm_read");
	exit(1);
    }
    addr = buf_addr;
    if (kvm_read(kd, addr, (char *) &buf_addr, sizeof(buf_addr)) == -1) {
	perror("kvm_read");
	exit(1);
    }

    /*
     * buf_addr is the pointer to the ill_t (interface linked list type ?? ).
     * Some members off ill_t are:
     * ill_next			Chained in at ill_g_head
     * ill_ipif_up_count	Number of IPIFs currently up
     * ill_max_frag		Max IDU
     * ill_name			Our name
     * ill_name_length		Name length, incl. terminator
     * ill_subnet_type		IRE_RESOLVER or IRE_SUBNET
     * ill_ppa			Physical Point of Attachment num
     * ill_sap			Service Access Point (normally ethertype)
     */
    addr = buf_addr;

    sprintf(if_name, "/dev/");
    do {
	if (kvm_read(kd, addr, (char *) &ill, sizeof(ill_t)) == -1) {
	    perror("kvm_read");
	    exit(1);
	}

	/* at this time only Ethernet interfaces are supported */
	if (ill.ill_max_frag != 1500) {
	    addr = (unsigned long) ill.ill_next;
	    continue;
	}

	/* ill.ill_name is a pointer to the interface name */
	addr = (unsigned long) ill.ill_name;
	if (kvm_read(kd, addr, if_name + 5, ill.ill_name_length) == -1) {
	    perror("kvm_read");
	    exit(1);
	}
	printf("%s: ", if_name);

	/* strip off trailing number(s) */
	strtok(if_name, "0123456789");
	if (strtok(NULL, "0123456789") != NULL) {
	    /* we don't accept names like foo6bar0 */
	    printf("NOT linked (strange name)\n");
	    continue;
	}

	if ((fdle = open(if_name, O_RDWR)) == -1) {
	    perror(if_name);
	    exit(1);
	}

	/* Attach, bind and link */
	dlattachreq(fdle, ill.ill_ppa);
	dlokack(fdle, buf);
	dlbindreq(fdle, ETHERTYPE_FLIP, 0, DL_CLDLS, 0, 0);
	dlbindack(fdle, buf);
	if (strioctl(fdle, DLIOCRAW, -1, 0, NULL) < 0)
	    syserr("DLIOCRAW");
	if (ioctl(fdflip, I_LINK, fdle) == -1) {
	    printf("I_LINK ioctl: ");
	    exit(1);
	}
	printf("linked\n");
	close(fdle);
	addr = (unsigned long) ill.ill_next;
    } while (addr != 0);
    printf("FLIP driver started\n");
    signal(SIGINT, handler);
    pause();

    /* All interfaces should be detached. Not implemented yet. */
    /* A better way might be to rewrite this daemon into a program */
    /* flipconfig. This can PLINK all interfaces. */
    /*
    dldetachreq(fdle);
    dlokack(fdle, buf);
    */
}
