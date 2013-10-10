#!/bin/sh
#set -x

if [ $# -ne 2 ]; then
	echo
	echo "`basename $0`: elf-file symbols-file"
	exit 1
fi

progname=$0
image_file=$1
symbol_file=$2

echo '/* This is an auto-generated file. Do not edit */'
echo '#include <asm/types.h>'

while read l
do
	#
	# Each entry is of the form
	# return-type symbol-name argument-list
	#

	# get the symbol name
	sym=`echo $l | cut -f1 '-d(' | sed 's/\s\+$//' | sed 's/^.*\s//'`

	# figure out the address
	addr=`nm -n $image_file | grep -w $sym | awk '{print "0x"$1}'`

	# change the function prototype to pointer declaration
	ll=`echo $l | sed "s/$sym/\\(\\*$sym\\)/" | sed 's/;//g'`

	# append the addres value
	echo $ll = '(void *)'$addr';'
done < $symbol_file
