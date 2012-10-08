/*	@(#)server.h	1.2	94/04/06 16:58:21 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#define MEMBERCAP	"MEMBERCAP"
#define GROUPCAP	"GROUPCAP"
#define PRIVATECAP	"PRIVATECAP"

/* request from client to server */
#define CP_CHECKPOINT	10
#define CP_ROLLBACK	11
#define CP_REMOVECP	12
#define CP_FINISH	13
#define CP_FLUSH	14

/* instructions from server to client */
#define CP_CONTINUE	20
#define CP_RESTART	21
