#!/bin/sh
numargs=$#
filesdir=$1
searchstr=$2

if [ $numargs -ne 2 ]
then
	echo usage: $0 filesdir searchstr
	return 1
fi

if [ ! -d $filesdir ]
then 
	echo $filesdir is not a directory
	return 1
fi

filesnum=$( ls $filesdir | wc -l )
linenum=$(grep -R $searchstr $filesdir | wc -l)

echo The number of files are $filesnum and the number of matching lines are $linenum
