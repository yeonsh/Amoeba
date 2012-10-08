#
#	
#	@(#)Hanoi.m4	1.2	94/04/06 18:08:05
#
# Copyright 1994 Vrije Universiteit, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Amoeba distribution.
#
define(hanoi, `trans(A, B, C, $1)')

define(moved,`move disk from $1 to $2
')

define(trans, `ifelse($4,1,`moved($1,$2)',
	`trans($1,$3,$2,DECR($4))moved($1,$2)trans($3,$2,$1,DECR($4))')')
