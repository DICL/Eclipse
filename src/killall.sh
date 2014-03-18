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
		echo "shutdown slave in raven0$i..."
		ssh raven0$i killall slave &
	else
		echo "shutdown slave in raven$i..."
		ssh raven$i killall slave  &
	fi
done

wait

echo -e "\033[0;32mDone\033[0m"

for((i=1; i<=$numslave; i++))
do
	if ((i<10))
	then
		echo "shutdown fileserver in raven0$i..."
		ssh raven0$i killall fileserver &
	else
		echo "shutdown fileserver in raven$i..."
		ssh raven$i killall fileserver &
	fi
done

wait

echo -e "\033[0;32mDone\033[0m"

for((i=1; i<=$numslave; i++))
do
	if ((i<10))
	then
		echo "shutdown $1 in raven0$i..."
		ssh raven0$i killall $1 &
	else
		echo "shutdown $1 in raven$i..."
		ssh raven$i killall $1 &
	fi
done

wait

echo -e "\033[0;32mDone\033[0m"

echo "shutdown master node..."

killall master
killall $1

echo -e "\033[0;32mDone\033[0m"
