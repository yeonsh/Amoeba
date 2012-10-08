/*	@(#)bs_protos.h	1.1	96/02/27 14:06:56 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __BS_PROTOS_H__
#define	__BS_PROTOS_H__

/* inode.c */
Inodenum	alloc_inode _ARGS((void));
void		inode_init _ARGS((void));
long		num_free_inodes _ARGS((void));
void		free_inode _ARGS((Inodenum,int,int,int));
void		lock_inode _ARGS((Inodenum, int));
int		trylock_inode _ARGS((Inodenum, int));
void		unlock_inode _ARGS((Inodenum));
int		inode_locked _ARGS((Inodenum));
void		destroy_inode _ARGS((Inodenum,int,int,int));
void		destroy_contents _ARGS((Inodenum,int));
void		set_inodes _ARGS((void));
void		update_inode _ARGS((Inodenum, struct Inode *));

/* b_misc.c */
void	bpanic _ARGS((char *, ...));
errstat	bs_supercap _ARGS((private *, rights_bits, int));
errstat	validate_cap _ARGS((private *, rights_bits, Inodenum *, int, int));
void	bwarn _ARGS((char *, ...));
int	bs_cap_eq _ARGS((capability *,capability *));
void	bs_log_send _ARGS((char *, int, int));

/* bs_adm_cmds.c */
errstat	bs_disk_compact _ARGS((header *));
errstat	bs_do_compact _ARGS((void));
errstat	bs_fsck _ARGS((header *));
errstat	bs_do_fsck _ARGS((void));
void	bs_status _ARGS((header *, int));
errstat	bs_sync _ARGS((header *));
errstat bs_do_sync _ARGS((void)) ;
errstat	bs_repchk _ARGS((header *));

/* gs_adm_cmds.c */
errstat	gs_disk_compact _ARGS((header *));
errstat	gs_fsck _ARGS((header *));
void	gs_status _ARGS((header *));
errstat	gs_sync _ARGS((header *));
errstat gs_repchk _ARGS((header *));
errstat	gs_state _ARGS((header *));

/* grp_std_cmds.c */
errstat	gs_std_setparams _ARGS((header *, bufptr, bufptr));
void	gs_std_getparams _ARGS((header *));

/* bs_user_cmds.c */
errstat	bs_create _ARGS((header *, bufptr, b_fsize));
errstat	bs_delete _ARGS((header *, bufptr, bufsize));
errstat	bs_insert _ARGS((header *, bufptr, b_fsize));
errstat	bs_modify _ARGS((header *, bufptr, b_fsize));
void	bs_read _ARGS((header *,int));
errstat	bs_size _ARGS((header *));

/* bs_std_cmds.c */
errstat	bs_std_age _ARGS((header *,int));
errstat	bs_std_copy _ARGS((header *, bufptr, bufptr));
errstat	bs_std_destroy _ARGS((header *));
void	bs_std_info _ARGS((header *, int));
errstat	bs_std_restrict _ARGS((private *, rights_bits, int));
errstat	bs_std_touch _ARGS((private *));
errstat bs_std_ntouch _ARGS((int,private *,bufsize *));
errstat	bs_std_setparams _ARGS((header *, bufptr, bufptr));
void	bs_std_getparams _ARGS((header *));
errstat bs_remote_read _ARGS((capability *, bufptr, b_fsize, b_fsize));

/* new_file.c */
errstat	bs_loadfile _ARGS((Cache_ent *,int));
errstat	clone_file _ARGS((Cache_ent **, b_fsize));
errstat	f_commit _ARGS((Cache_ent *, int, private *));
void	dispose_file _ARGS((Cache_ent *));
errstat f_todisk _ARGS((Cache_ent *, int));

/* kernel_/user_dep.c */
int	get_disk_server_cap _ARGS((void));
void	bs_start_service _ARGS((void));
errstat	bs_disk_read _ARGS((disk_addr, bufptr, disk_addr));
errstat	bs_disk_write _ARGS((bufptr, disk_addr, disk_addr));
void	gbullet_init_all _ARGS((void));

/* readwrite.c */
int	valid_superblk _ARGS((Superblk *, Superblk *));
void	read_inodetab _ARGS((disk_addr));
void	write_inodetab _ARGS((disk_addr));
void	writecache _ARGS((Cache_ent *,int));
void	writefile _ARGS((Cache_ent *));
void	lock_ino_cache _ARGS((Inodenum));
void	unlock_ino_cache _ARGS((Inodenum));
void	writeinode _ARGS((Inode *,int,int));
void	write_superblk _ARGS((Superblk *));
void	bs_init_irw _ARGS((void)) ;
void	bs_block_flush _ARGS((void)) ;

/* shuffle.c */
void	bs_shuffle_free _ARGS((void));

/* bullet_init.c */
int	bullet_init _ARGS((void));
int	bullet_start _ARGS((void));
void	bullet_register _ARGS((void));
void	super_lock _ARGS((void));
void	super_free _ARGS((void));

/* bullet.c */
#if defined(USER_LEVEL)
void	bullet_thread _ARGS((char *, int));
void	super_thread _ARGS((char *, int));
#else  /* Kernel version */
void	bullet_thread _ARGS((void));
void	super_thread _ARGS((void));
#endif

/* member.c */
#if defined(USER_LEVEL)
void	member_thread _ARGS((char *, int));
#else  /* Kernel version */
void	member_thread _ARGS((void));
#endif

/* grp_adm.c */
errstat	gs_attach _ARGS((header *, bufptr, bufptr, int));
errstat	bs_state _ARGS((header *));
errstat bs_do_state _ARGS((header *,int));
void	gs_welcome _ARGS((header *, bufptr, bufptr));
char	*member_status _ARGS((char *, char *));
char	*grp_status _ARGS((char *, char *));
char	*bs_can_start_service _ARGS((void));
char	*bs_grp_ms _ARGS((int));
errstat	bs_retreat _ARGS((int));

/* grp_bullet.c */
void	bs_grp_init _ARGS((void)) ;
void	bs_print_lock _ARGS((void)) ;
void	bs_print_unlock _ARGS((void)) ;
int	bs_print_trylock _ARGS((interval)) ;
errstat bs_member_read _ARGS((Inodenum, struct Inode_cache *, bufptr buf,
				b_fsize)) ;
errstat bs_grp_defer _ARGS((Inodenum, Inode *, int)) ;
errstat bs_new_inode _ARGS((Cache_ent **)) ;
void	bs_grp_sweep _ARGS((void)) ;
void	bs_version_incr _ARGS((Inode *));
int	bs_grp_missing _ARGS((void));
errstat bs_grp_all _ARGS((header *, bufptr,int)) ;
errstat bs_grp_single _ARGS((int,header *, bufptr,int)) ;
errstat gs_chsuper _ARGS((int,int,int,int,int,capability *));
errstat bs_change_status _ARGS((int,int, capability *,int));
void	bs_create_mode _ARGS((int));
void	bs_set_state _ARGS((int));
void	bs_check_state _ARGS((void));
void	bs_update _ARGS((header *));
void    bs_grpreq _ARGS((header *hdr, char *buf, int size, int mode));
int	bs_has_majority _ARGS((void));
#if defined(USER_LEVEL)
void	bullet_back_sweeper _ARGS((char *, int)) ;
void	bullet_send_queue _ARGS((char *, int)) ;
#else  /* Kernel version */
void	bullet_back_sweeper _ARGS((void)) ;
void	bullet_send_queue _ARGS((void)) ;
#endif

/* replic.c */
errstat	gs_startrep _ARGS((Cache_ent *)) ;
errstat	gs_replicate _ARGS((header *, bufptr, b_fsize));
errstat	bs_do_repchk _ARGS((void)) ;
void	gs_sweeprep _ARGS((void)) ;
void	gs_lostrep _ARGS((void)) ;
void	gs_flagnew _ARGS((int)) ;
void	gs_rep_init _ARGS((void)) ;

/* aging.c */
errstat	gs_reset_ttl _ARGS((Inodenum)) ;
void	gs_set_ttl _ARGS((Inodenum)) ;
void	gs_force_ttl _ARGS((Inodenum)) ;
void	gs_clear_ttl _ARGS((Inodenum)) ;
void	gs_init_ttl _ARGS((void)) ;
void	gs_first_ttl _ARGS((void)) ;
void	gs_later_ttl _ARGS((void)) ;
void	gs_clear_ttl _ARGS((Inodenum)) ;
errstat	gs_std_age _ARGS((header *));
void	gs_do_age _ARGS((header *)) ;
void	gs_setlifes _ARGS((header *, char *, int)) ;
void	gs_checklife _ARGS((header *)) ;
void	gs_send_ttls _ARGS((void)) ;
void	gs_age_sweep _ARGS((void)) ;
void	gs_memb_crash _ARGS((void)) ;
void	gs_allow_aging _ARGS((void));
void	gs_delay_age _ARGS((header *)) ;
void	bs_allow_ages _ARGS((void));
void	gs_age_exit _ARGS((void));

/* debugging */
/* Whether to include debugging at compilation time */
#define DEBUG_MSGS	1
/* The default level, changeable by setparams */
#define DEBUG_LEVEL	6
#ifdef DEBUG_MSGS
extern int	bs_debug_level	;
#define dbmessage(lev, list) if (lev <= bs_debug_level) { message list; }
#else
#define dbmessage(lev, list)	/* Nothing */
#endif

void	message _ARGS((char *format, ...)) ;

/* A horrible hack */
#define STD_TODO	STD_DENIED

/* Externals */

#endif /* __BS_PROTOS_H__ */
