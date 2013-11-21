#awk 'BEGIN{LINE=0}{LINE++}END{print "Total number of lines: " LINE}' \
wc -l \
\
Makefile \
run.sh \
countline.sh \
setup.conf \
\
bin/Makefile \
\
client/Makefile \
client/client.cc \
client/client.hh \
\
mapreduce/definitions.hh \
mapreduce/job.hh \
mapreduce/mapreduce.hh \
mapreduce/task.hh \
\
master/Makefile \
master/connclient.hh \
master/connslave.hh \
master/dec_connclient.hh \
master/master.cc \
master/master.hh \
\
mcc/Makefile \
mcc/mcc.cc \
mcc/mcc.hh \
\
slave/Makefile \
slave/slave.cc \
slave/slave.hh
