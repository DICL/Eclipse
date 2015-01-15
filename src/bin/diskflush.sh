for line in `cat nodelist.conf`
do
	ssh $line java -classpath /home/youngmoon01/DEMA/qlb/diskflush Flush &
done

wait
