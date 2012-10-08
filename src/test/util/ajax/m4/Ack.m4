#
#	
#	@(#)Ack.m4	1.2	94/04/06 18:01:32
#
# Copyright 1994 Vrije Universiteit, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Amoeba distribution.
#
define(ack, `ifelse($1,0,incr($2),$2,0,`ack(DECR($1),1)',
`ack(DECR($1), ack($1,DECR($2)))')')
