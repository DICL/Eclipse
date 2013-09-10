#usage: sh run.sh [number of slave nodes]
if(($#==0))
then
	echo "usage: sh run.sh [number of slave nodes]"
	exit
fi

echo "launching master"
sh $MRR_HOME/bin/runmaster.sh $1 &

sleep 1

for((i=1; i<=$1; i++))
do
	if ((i<10))
	then
		echo "launching slave $i"
		ssh raven0$i sh $MRR_HOME/bin/runslave.sh ravenleader &
	else
		echo "launching slave $i"
		ssh raven$i sh $MRR_HOME/bin/runslave.sh ravenleader &
	fi
done

wait
