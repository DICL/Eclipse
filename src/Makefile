all:
	echo Type ether: [make nfs] or [make hdfs]

nfs: $(MR_HOME)/bin/master $(MR_HOME)/bin/slave $(MR_HOME)/bin/client $(MR_HOME)/mcc/mcc_nfs.cc $(MR_HOME)/mcc/mcc_nfs.hh $(MR_HOME)/mapreduce/nfs_hh/mapreduce.hh 
	g++ $(MR_HOME)/mcc/mcc_nfs.cc -o $(MR_HOME)/bin/mcc  -lpthread -I $(MR_HOME) -Wall

hdfs: $(MR_HOME)/bin/master $(MR_HOME)/bin/slave $(MR_HOME)/bin/client $(MR_HOME)/mcc/mcc_hdfs.cc $(MR_HOME)/mcc/mcc_hdfs.hh $(MR_HOME)/mapreduce/hdfs_hh/mapreduce.hh
	g++ $(MR_HOME)/mcc/mcc_hdfs.cc -o $(MR_HOME)/bin/mcc -lpthread -I $(MR_HOME) -Wall

$(MR_HOME)/bin/master: $(MR_HOME)/master/master.cc $(MR_HOME)/master/master.hh $(MR_HOME)/mapreduce/definitions.hh $(MR_HOME)/master/master_job.hh $(MR_HOME)/master/master_task.hh $(MR_HOME)/master/connslave.hh $(MR_HOME)/master/connclient.hh
	g++ $(MR_HOME)/master/master.cc -o $(MR_HOME)/bin/master -lpthread -I $(MR_HOME) -Wall

$(MR_HOME)/bin/slave: $(MR_HOME)/slave/slave.cc $(MR_HOME)/slave/slave.hh $(MR_HOME)/mapreduce/definitions.hh $(MR_HOME)/slave/slave_job.hh $(MR_HOME)/slave/slave_task.hh
	g++ $(MR_HOME)/slave/slave.cc -o $(MR_HOME)/bin/slave -lpthread -I $(MR_HOME) -Wall

$(MR_HOME)/bin/client: $(MR_HOME)/client/client.cc $(MR_HOME)/client/client.hh $(MR_HOME)/mapreduce/definitions.hh
	g++ $(MR_HOME)/client/client.cc -o $(MR_HOME)/bin/client -lpthread -I $(MR_HOME) -Wall

clean:
	rm -f $(MR_HOME)/bin/master
	rm -f $(MR_HOME)/bin/slave
	rm -f $(MR_HOME)/bin/client
	rm -f $(MR_HOME)/bin/mcc
