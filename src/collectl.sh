for (( i=21; i<=36; i++ ))
do
	echo raven$i
	ssh raven$i collectl &
done

wait
