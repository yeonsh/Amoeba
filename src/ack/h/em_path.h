/*	@(#)em_path.h	1.3	96/02/27 09:59:36 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* Intended as a common directory for ALL temporary files */
#define TMP_DIR		"/tmp"

/* Access to the ACK tree and parts thereof */
#ifdef AMOEBA
#define EM_DEFAULT_DIR	"/profile/module/ack"
#else
#define EM_DEFAULT_DIR	"/usr/proj/em/Work"
#endif

#ifndef EM_DIR
#define EM_DIR		EM_DEFAULT_DIR	/* The root directory for EM stuff */
#endif

#define RTERR_PATH	"etc/pc_rt_errors"
#define ACK_PATH	"lib/descr"
