NODELIST=$(cat $ECLIPSE_PATH/etc/eclipse.json | $ECLIPSE_PATH/bin/jsawk 'return this.nodes.join("\n")')

i=0
for line in ${NODELIST[@]}
do
  echo "Shutting down slave in node $i"
  ssh $line killall slave &
  (( i++ ))
done

wait

echo -e "\033[0;32mDone\033[0m"

i=0
for line in ${NODELIST[@]}
do
  echo "Shutting down eclipse in node $i"
  ssh $line killall eclipse &
  (( i++ ))
done

wait

echo -e "\033[0;32mDone\033[0m"

for program in $*
do
  i=0
  for line in ${NODELIST[@]}
  do
    echo "Shutting down $program in node $i"
    ssh $line killall $program &
    (( i++ ))
  done
  wait
done

echo -e "\033[0;32mDone\033[0m"

echo "Shutting down master node..."

killall master
killall cacheserver

for program in $*
do
  killall $program
done

echo -e "\033[0;32mDone\033[0m"
