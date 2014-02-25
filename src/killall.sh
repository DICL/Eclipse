if (($#<1))
then
	echo "Usage: sh kill.sh [application program name]"
	exit
fi

numslave=$(awk '$1=="num_slave"{print $2}' setup.conf)
for((i=1; i<=$numslave; i++))
do
	if ((i<10))
	then
		echo "killing slave in raven0$i"
		ssh raven0$i killall slave &
	else
		echo "killing slave in raven$i"
		ssh raven$i killall slave  &
	fi
done

wait

for((i=1; i<=$numslave; i++))
do
	if ((i<10))
	then
		echo "killing $1 in raven0$i"
		ssh raven0$i killall $1 &
	else
		echo "killing $1 in raven$i"
		ssh raven$i killall $1 &
	fi
done

killall master

wait
