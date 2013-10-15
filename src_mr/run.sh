#usage: sh run.sh [number of slave nodes]
if(($#==0))
then
	echo "usage: sh run.sh [number of slave nodes]"
	exit
fi

echo "launching master"
$MR_HOME/bin/master $1 &

# sleep 1 seconds to ensure the initialization of master before slave starts
sleep 1

for((i=1; i<=$1; i++))
do
	if ((i<10))
	then
		echo "launching slave $i"
		ssh raven0$i $MR_HOME/bin/slave ravenleader &
	else
		echo "launching slave $i"
		ssh raven$i $MR_HOME/bin/slave ravenleader &
	fi
done

wait
