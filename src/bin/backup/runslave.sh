#usage: sh runslave.sh [server ip]

if(($# == 0))
then
	echo "usage: sh runslave.sh [server ip]"
	exit
fi
$MRR_HOME/bin/slave $1
