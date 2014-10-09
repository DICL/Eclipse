collectl &

for (( i=1; i<=38; i++ ))
do
	if (( i < 10 ))
	then
		ssh raven0$i collectl &
	else
		ssh raven$i collectl &
	fi
done

wait
