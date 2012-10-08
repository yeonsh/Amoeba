/*	@(#)grp_comm.h	1.1	96/02/27 14:07:20 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * GRP_COMM.H: generic group communication module.
 */

void    grp_init_port _ARGS((int id, port *group_p, capability *member_c));
void    grp_startup   _ARGS((int min_copies, int nmembers));
void	bs_grp_trysend _ARGS((header *hdr, char *buf, int size, int mode));
void    grp_recovery_thread _ARGS((void));
char	*grp_mem_status _ARGS((int member));
int	grp_member_present _ARGS((int member));
errstat	grp_pass_request _ARGS((int member));
void	bs_leave_group _ARGS((void));
void	bs_be_zombie _ARGS((void));
void	bs_gc_init _ARGS((void)) ;

/* Externals */
int	grp_init_create _ARGS((void));

/* The identifying number of the protocol */
#define BS_PROTOCOL	1

/* The maximum size of group requests */
#define BS_GRP_REQSIZE	((8 * 1024)- sizeof(header))

/* Flag that indicates whether to respond to out-of-date inodes */
int	bs_mopup ;
