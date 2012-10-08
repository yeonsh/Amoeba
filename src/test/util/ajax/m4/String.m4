#
#	
#	@(#)String.m4	1.2	94/04/06 17:59:20
#
# Copyright 1994 Vrije Universiteit, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Amoeba distribution.
#
define(string,`integer $1(len(substr($2,1)))
str($1,substr($2,1),0)
data $1(len(substr($2,1)))/EOS/
')

define(str,`ifelse($2,",,data $1(incr($3))/`LET'substr($2,0,1)/
`str($1,substr($2,1),incr($3))')')
