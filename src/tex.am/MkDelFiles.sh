#!/bin/sh
#	
#	@(#)MkDelFiles.sh	1.1	92/05/13 16:20:14
#
#
# MkDeleteFiles - Make a script DeleteFiles that will
# delete all files from another hierarchy that are in this hierarchy.
#
rm -f DeleteFiles.sh
cat >DeleteFiles.sh <<FOOBIE
#!/bin/sh
if [ -f DeleteFiles.sh ]; then
    echo OOPS! You are running DeleteFiles in the wrong directory!
    exit
fi
FOOBIE
find . ! -type d -print | sed -e 's/^/rm -f /' >> DeleteFiles.sh
chmod 755 DeleteFiles.sh
