#usage: sh run.sh

echo "Launching master..."
$ECLIPSE_PATH/bin/master &

# sleep 1 seconds to ensure the initialization of master before slave starts
sleep 1

#numslave=$(awk '$1=="num_slave"{print $2}' setup.conf)

i=0
for line in `cat $ECLIPSE_PATH/nodelist.conf`
do
	echo "Launching slave $i"
	ssh $line 'export ECLIPSE_PATH="'"$ECLIPSE_PATH"'"; '$ECLIPSE_PATH'/bin/slave' &
	(( i++ ))
done

#if [ -e $MR_HOME/make_version/dht_mode ]
#then
	echo "Launching cache server"
	$ECLIPSE_PATH/bin/cacheserver &

	i=0
	for line in `cat $ECLIPSE_PATH/nodelist.conf`
	do
		echo "Launching eclipse $i"
		ssh $line 'export ECLIPSE_PATH="'"$ECLIPSE_PATH"'"; '$ECLIPSE_PATH'/bin/eclipse' &
		(( i++ ))
	done
#fi

wait
