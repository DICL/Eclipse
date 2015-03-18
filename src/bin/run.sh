#usage: sh run.sh

echo "Launching master..."
$ECLIPSE_PATH/bin/master &
NODELIST=$(cat $ECLIPSE_CONF_PATH/eclipse.json | $ECLIPSE_PATH/bin/jsawk 'return this.nodes.join("\n")')

sleep 1

i=0
for line in ${NODELIST[@]}
do
  echo "Launching slave $i"
  ssh $line 'export ECLIPSE_PATH="'"$ECLIPSE_PATH"'"; '$ECLIPSE_PATH'/bin/slave' &
  (( i++ ))
done

echo "Launching cache server"
$ECLIPSE_PATH/bin/cacheserver &

i=0
for line in ${NODELIST[@]}
do
  echo "Launching eclipse $i"
  ssh $line 'export ECLIPSE_PATH="'"$ECLIPSE_PATH"'"; '$ECLIPSE_PATH'/bin/eclipse' &
  (( i++ ))
done

wait
