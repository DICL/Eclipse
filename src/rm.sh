# remove the existing input files
for line in `cat nodelist.conf`
do
	ssh $line mrrm &
done

wait
