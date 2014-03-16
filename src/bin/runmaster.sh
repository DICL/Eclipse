#usage: sh runmaster.sh [number of client servers]

if(($# == 0))
then
	echo "usage: sh runmaster.sh [number of client servers]"
	exit
fi
$MRR_HOME/bin/master $1
