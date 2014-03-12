#usage: sh run.sh

echo "launching master"
$MR_HOME/bin/master &

# sleep 1 seconds to ensure the initialization of master before slave starts
sleep 1

numslave=$(awk '$1=="num_slave"{print $2}' setup.conf)

for((i=1; i<=$numslave; i++))
do
	if ((i<10))
	then
		echo "launching slave $i"
		ssh raven0$i $MR_HOME/bin/slave &
	else
		echo "launching slave $i"
		ssh raven$i $MR_HOME/bin/slave &
	fi
done

wait
