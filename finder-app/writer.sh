numargs=$#
writefile=$1
writestr=$2

if [ $numargs -ne 2 ]
then
	echo usage: $0 writefile writestr
	return 1
fi

mkdir -p "$(dirname "$writefile")"
echo $writestr > $writefile
