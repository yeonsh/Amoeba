/*	@(#)bs_protos.h	1.6	96/02/27 14:11:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __BS_PROTOS_H__
#define	__BS_PROTOS_H__

/* inode.c */
Inodenum	alloc_inode _ARGS((void));
void		inode_init _ARGS((void));
long		num_free_inodes _ARGS((void));
void		free_inode _ARGS((Inodenum));
void		lock_inode _ARGS((Inodenum, int));
void		unlock_inode _ARGS((Inodenum));
int		inode_locked _ARGS((Inodenum));
void		destroy_inode _ARGS((Inodenum));

/* b_misc.c */
void	bpanic _ARGS((char *, ...));
errstat	bs_supercap _ARGS((private *, rights_bits));
errstat	validate_cap _ARGS((private *, rights_bits, Inodenum *, int));
void	bwarn _ARGS((char *, ...));

/* bs_adm_cmds.c */
errstat	bs_disk_compact _ARGS((header *));
errstat	bs_fsck _ARGS((header *));
void	bs_status _ARGS((header *));
errstat	bs_sync _ARGS((header *));

/* bs_user_cmds.c */
errstat	bs_create _ARGS((header *, bufptr, b_fsize));
errstat	bs_delete _ARGS((header *, bufptr, bufsize));
errstat	bs_insert _ARGS((header *, bufptr, b_fsize));
errstat	bs_modify _ARGS((header *, bufptr, b_fsize));
void	bs_read _ARGS((header *));
errstat	bs_size _ARGS((header *));

/* bs_std_cmds.c */
errstat	bs_std_age _ARGS((header *));
errstat	bs_std_copy _ARGS((header *, bufptr, bufptr));
errstat	bs_std_destroy _ARGS((header *));
void	bs_std_info _ARGS((header *));
errstat	bs_std_restrict _ARGS((private *, rights_bits));
errstat	bs_std_touch _ARGS((private *));
errstat	bs_std_ntouch _ARGS((bufsize, private *, bufsize *));

/* new_file.c */
errstat	bs_loadfile _ARGS((Cache_ent *));
errstat	clone_file _ARGS((Cache_ent **, b_fsize));
errstat	f_commit _ARGS((Cache_ent *, int, private *));
void	dispose_file _ARGS((Cache_ent *));

/* kernel_/user_dep.c */
int	get_disk_server_cap _ARGS((void));
errstat	bs_disk_read _ARGS((disk_addr, bufptr, disk_addr));
errstat	bs_disk_write _ARGS((bufptr, disk_addr, disk_addr));
void	bullet_init_all _ARGS((void));

/* readwrite.c */
int	valid_superblk _ARGS((void));
void	read_inodetab _ARGS((disk_addr));
void	writecache _ARGS((Cache_ent *));
void	writefile _ARGS((Cache_ent *));
void	writeinode _ARGS((Inode *));

/* bullet_init.c */
int	bullet_init _ARGS((void));

/* bullet.c */
#if defined(USER_LEVEL)
void	bullet_thread _ARGS((char *, int));
#else
void	bullet_thread _ARGS((void));
#endif

#endif /* __BS_PROTOS_H__ */
