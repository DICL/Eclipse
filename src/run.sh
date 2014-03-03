#usage: sh run.sh

# transfer the text files if dht mode is built
if [ -e $MR_HOME/make_version/dht_mode ]
then
	fd
fi

echo "Launching master..."
$MR_HOME/bin/master &

# sleep 1 seconds to ensure the initialization of master before slave starts
sleep 1

numslave=$(awk '$1=="num_slave"{print $2}' setup.conf)

for((i=1; i<=$numslave; i++))
do
	if ((i<10))
	then
		echo "Launching slave $i"
		ssh raven0$i $MR_HOME/bin/slave &
	else
		echo "Launching slave $i"
		ssh raven$i $MR_HOME/bin/slave &
	fi
done

wait
