rm -f /scratch/youngmoon01/mr_storage/.job*

for line in `cat nodelist.conf`
do
	ssh $line rm -f $DHT_PATH/.job* &
done

wait
