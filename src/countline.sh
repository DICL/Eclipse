#awk 'BEGIN{LINE=0}{LINE++}END{print "Total number of lines: " LINE}' \
wc -l \
\
Makefile \
countline.sh \
run.sh \
setup.conf \
\
client/Makefile \
client/client.cc \
client/client.hh \
\
mapreduce/definitions.hh \
mapreduce/mapreduce.hh \
\
master/Makefile \
master/connclient.hh \
master/connslave.hh \
master/dec_connclient.hh \
master/master.cc \
master/master.hh \
master/master_job.hh \
master/master_task.hh \
\
mcc/Makefile \
mcc/mcc.cc \
mcc/mcc.hh \
\
slave/Makefile \
slave/slave.cc \
slave/slave.hh
