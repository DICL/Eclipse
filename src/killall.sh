if (($#<1))
then
	echo "Usage: sh kill.sh [application program name]"
	exit
fi

i=0
for line in `cat nodelist.conf`
do
	echo "Shutting down slave in node $i"
	ssh $line killall slave &
	(( i++ ))
done

wait

echo -e "\033[0;32mDone\033[0m"

i=0
for line in `cat nodelist.conf`
do
	echo "Shutting down eclipse in node $i"
	ssh $line killall eclipse &
	(( i++ ))
done

wait

echo -e "\033[0;32mDone\033[0m"

i=0
for line in `cat nodelist.conf`
do
	echo "Shutting down $1 in node $i"
	ssh $line killall $1 &
	(( i++ ))
done

wait

echo -e "\033[0;32mDone\033[0m"

echo "Shutting down master node..."

killall master
killall $1

echo -e "\033[0;32mDone\033[0m"
