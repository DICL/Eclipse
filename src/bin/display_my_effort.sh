cat Makefile > garbage.txt
cat countline.sh >> garbage.txt
cat run.sh >> garbage.txt
cat run_without_fd.sh >> garbage.txt
cat clear_mr.sh >> garbage.txt
cat killall.sh >> garbage.txt
cat setup.conf >> garbage.txt
cat bin/fd >> garbage.txt
cat bin/mrls >> garbage.txt
cat bin/mrrm >> garbage.txt
cat bin/mrcat >> garbage.txt
cat client/client.cc >> garbage.txt
cat client/client.hh >> garbage.txt
cat mapreduce/definitions.hh >> garbage.txt
cat mapreduce/nfs/mapreduce.hh >> garbage.txt
cat mapreduce/hdfs/mapreduce.hh >> garbage.txt
cat mapreduce/dht/mapreduce.hh >> garbage.txt
cat master/master.cc >> garbage.txt
cat master/master.hh >> garbage.txt
cat master/dht/master.cc >> garbage.txt
cat master/dht/master.hh >> garbage.txt
cat master/connclient.hh >> garbage.txt
cat master/connslave.hh >> garbage.txt
cat master/master_job.hh >> garbage.txt
cat master/master_task.hh >> garbage.txt
cat mcc/nfs/mcc.cc >> garbage.txt
cat mcc/nfs/mcc.hh >> garbage.txt
cat mcc/hdfs/mcc.cc >> garbage.txt
cat mcc/hdfs/mcc.hh >> garbage.txt
cat mcc/dht/mcc.cc >> garbage.txt
cat mcc/dht/mcc.hh >> garbage.txt
cat slave/slave.cc >> garbage.txt
cat slave/slave.hh >> garbage.txt
cat slave/dht/slave.cc >> garbage.txt
cat slave/dht/slave.hh >> garbage.txt
cat slave/slave_job.hh >> garbage.txt
cat slave/slave_task.hh >> garbage.txt
cat common/fileclient.hh >> garbage.txt
cat common/msgaggregator.hh >> garbage.txt
cat file_distributor/writecount.hh >> garbage.txt
cat file_distributor/iwriter.hh >> garbage.txt
cat file_distributor/idistributor.hh >> garbage.txt
cat file_distributor/ireader.hh >> garbage.txt
cat file_distributor/fd_core.cc >> garbage.txt
cat file_distributor/mrcat_core.cc >> garbage.txt
cat file_distributor/fileserver.hh >> garbage.txt
cat file_distributor/filebridge.hh >> garbage.txt
cat file_distributor/filepeer.hh >> garbage.txt
cat file_distributor/file_connclient.hh >> garbage.txt
cat file_distributor/messagebuffer.hh >> garbage.txt
cat orthrus/launcher.cc >> garbage.txt
cat orthrus/cacheserver.cc >> garbage.txt
cat orthrus/iwfrequest.hh >> garbage.txt
cat orthrus/histogram.hh >> garbage.txt
cat orthrus/datablock.hh >> garbage.txt
cat orthrus/dataentry.hh >> garbage.txt
cat orthrus/cache.h>> garbage.txt

IFS=" "
SPEEDUP=5
GROUPBY=20

r=$RANDOM
sleeptime=`echo "$r/(32767.0*$SPEEDUP)" | bc -l`
n=0

while read line
do
	echo -e "$line"

	if (( n == $GROUPBY ))
	then
		r=$RANDOM
		sleeptime=`echo "$r/(32767.0*$SPEEDUP)" | bc -l`
		n=0
	fi

	sleep $sleeptime
	(( n++ ))

done < garbage.txt

rm garbage.txt
