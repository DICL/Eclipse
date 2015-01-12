#usage: sh run.sh

echo "Launching master..."
$MR_HOME/bin/master &

# sleep 1 seconds to ensure the initialization of master before slave starts
sleep 1

#numslave=$(awk '$1=="num_slave"{print $2}' setup.conf)

i=0
for line in `cat $MR_HOME/nodelist.conf`
do
	echo "Launching slave $i"
	ssh $line 'export ECLIPSE_PATH="'"$ECLIPSE_PATH"'"; '$MR_HOME'/bin/slave' &
	(( i++ ))
done

#if [ -e $MR_HOME/make_version/dht_mode ]
#then
	echo "Launching cache server"
	$MR_HOME/bin/cacheserver &

	i=0
	for line in `cat $MR_HOME/nodelist.conf`
	do
		echo "Launching eclipse $i"
		ssh $line 'export ECLIPSE_PATH="'"$ECLIPSE_PATH"'"; '$MR_HOME'/bin/eclipse' &
		(( i++ ))
	done
#fi

wait
