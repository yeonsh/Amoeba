#
#	
#	@(#)Sqroot.m4	1.2	94/04/06 17:59:14
#
# Copyright 1994 Vrije Universiteit, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Amoeba distribution.
#
define(square_root, 
	`ifelse(eval($1<0),1,negative-square-root,
			     `square_root_aux($1, 1, eval(($1+1)/2))')')
define(square_root_aux,
	`ifelse($3, $2, $3,
		$3, eval($1/$2), $3,
		`square_root_aux($1, $3, eval(($3+($1/$3))/2))')')
