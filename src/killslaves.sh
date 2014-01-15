for((i=1; i<=39; i++))
do
	if ((i<10))
	then
		echo "killing slave in raven0$i"
		ssh raven0$i killall slave &
	else
		echo "killing slave in raven$i"
		ssh raven$i killall slave &
	fi
done

wait
