#!/bin/sh
#	
#	@(#)Tfromb.sh	1.2	94/04/06 17:59:42
#
# Copyright 1994 Vrije Universiteit, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Amoeba distribution.
#
# A shell script for testing the fromb (and tob) commands:

# $TEST_BEGIN "$0"
echo "Starting Bullet test (this will take 5 minutes)"

echo "ABCDEFGHIJKLMNOPQRSTUVWXYZ"	>/tmp/Ttob_a$$
echo "abcdefghijklmnopqrstuvwxyz"	>/tmp/Ttob_b$$
echo "0123456789" 			>/tmp/Ttob_c$$
cat $HOME/.profile			>/tmp/Ttob_d$$
cat /etc/passwd				>/tmp/Ttob_e$$
(date ; sleep 1; date ; sleep 1; date)	>/tmp/Ttob_f$$
/bin/ls -lis				>/tmp/Ttob_g$$
echo "Every good boy does fine. The
quick brown fox jumped over the lazy
dogs."					>/tmp/Ttob_h$$
sort $HOME/.profile			>/tmp/Ttob_i$$
sed -e 's/.*/& &/' $HOME/.profile	>/tmp/Ttob_j$$

if [ -f /Environ ]
then	TOB=cp		# Amoeba
else	TOB=tob		# Unix
fi

for ch in a b c d e f g h i j ; do
	if	$TOB /tmp/Ttob_$ch$$ /tmp/test_bullet$ch
	then	:;
	else	echo "$TOB /tmp/Ttob_$ch$$ /tmp/test_bullet$ch failed." >&2
	fi
done
#dir | grep test_bullet

# Delete every other file:
for ch in b d f h j ; do
	if	del -f /tmp/test_bullet$ch
	then	:;
	else	echo "del -f /tmp/test_bullet$ch failed." >&2
	fi
done
#dir /tmp | grep test_bullet

# Sleep five minutes
sleep 300

# Read back the remaining files and compare with originals:
for ch in a c e g i ; do
	if	fromb /tmp/test_bullet$ch >/tmp/Tfromb_$ch$$
	then	:;
	else	echo "\`fromb /tmp/test_bullet >/tmp/Tfromb_$ch$$' failed." >&2
	fi
	if [ $? -eq 0 ] ; then
		if	cmp /tmp/Ttob_$ch$$ /tmp/Tfromb_$ch$$
		then	:;
		else	echo "Output of fromb not equal to input of tob." >&2
		fi
	fi
	del -f /tmp/test_bullet$ch 
done

/bin/rm /tmp/Tfromb_*$$ /tmp/Ttob_*$$

# $TEST_END
