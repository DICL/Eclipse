#DHT_OBJ = $(MR_HOME)/DHT/DHTserver.o
#DHT_OBJ += $(MR_HOME)/DHT/DHTclient.o
MR_DEF = $(MR_HOME)/mapreduce/definitions.hh 
MASTER_DEP = $(MR_HOME)/master/master_job.hh 
MASTER_DEP += $(MR_HOME)/master/master_task.hh 
MASTER_DEP += $(MR_HOME)/master/connslave.hh 
MASTER_DEP += $(MR_HOME)/master/connclient.hh
SLAVE_DEP = $(MR_HOME)/slave/slave_job.hh
SLAVE_DEP += $(MR_HOME)/slave/slave_task.hh
ECLIPSE_DEP = $(MR_HOME)/file_distributor/fileserver.hh
ECLIPSE_DEP += $(MR_HOME)/file_distributor/filepeer.hh 
ECLIPSE_DEP += $(MR_HOME)/file_distributor/filebridge.hh 
ECLIPSE_DEP += $(MR_HOME)/file_distributor/messagebuffer.hh 
ECLIPSE_DEP += $(MR_HOME)/file_distributor/file_connclient.hh 
ECLIPSE_DEP += $(MR_HOME)/orthrus/histogram.hh 
CACHESERVER_DEP = $(MR_HOME)/orthrus/histogram.hh
CACHESERVER_DEP += $(MR_HOME)/orthrus/cacheserver.cc
CACHESERVER_DEP += $(MR_HOME)/orthrus/cacheclient.hh
DHT_FLAG = -I $(MR_HOME)/DHT/
DHT_FLAG += -I $(MR_HOME)/common/ 
DHT_FLAG += $(DHT_OBJ)
DHT_FLAG += -L $(MR_HOME)/common/
DHT_FLAG += -lmrr
CCFLAGS = -Wall


all:
	echo Type ether: [make nfs], [make hdfs], [make dht] [make clean]

dht: $(MR_HOME)/make_version/master_dht $(MR_HOME)/make_version/slave_dht $(MR_HOME)/make_version/client_dht $(MR_HOME)/make_version/mcc_dht $(MR_HOME)/bin/fd_core $(MR_HOME)/common/hash.o $(MR_HOME)/bin/eclipse $(MR_HOME)/bin/cacheserver
	touch $(MR_HOME)/make_version/dht_mode

nfs: $(MR_HOME)/make_version/master_nfs_hdfs $(MR_HOME)/make_version/slave_nfs_hdfs $(MR_HOME)/make_version/client_nfs_hdfs $(MR_HOME)/make_version/mcc_nfs $(MR_HOME)/common/hash.o
	rm -f $(MR_HOME)/make_version/dht_mode

hdfs: $(MR_HOME)/make_version/master_nfs_hdfs $(MR_HOME)/make_version/slave_nfs_hdfs $(MR_HOME)/make_version/client_nfs_hdfs $(MR_HOME)/make_version/mcc_hdfs $(MR_HOME)/common/hash.o
	rm -f $(MR_HOME)/make_version/dht_mode


$(MR_HOME)/make_version/master_dht: $(MR_HOME)/master/dht/master.cc $(MR_HOME)/master/dht/master.hh $(MR_DEF) $(MASTER_DEP) $(DHT_OBJ) $(MR_HOME)/make_version/master $(MR_HOME)/common/hash.o
	g++ $(CCFLAGS) $(MR_HOME)/master/dht/master.cc -o $(MR_HOME)/bin/master -I $(MR_HOME) $(DHT_FLAG) -lpthread 
	touch $(MR_HOME)/make_version/master
	touch $(MR_HOME)/make_version/master_dht

$(MR_HOME)/make_version/master_nfs_hdfs: $(MR_HOME)/master/master.cc $(MR_HOME)/master/master.hh $(MR_DEF) $(MASTER_DEP) $(MR_HOME)/make_version/master
	g++ $(CCFLAGS) $(MR_HOME)/master/master.cc -o $(MR_HOME)/bin/master -I $(MR_HOME) -lpthread 
	touch $(MR_HOME)/make_version/master
	touch $(MR_HOME)/make_version/master_nfs_hdfs


$(MR_HOME)/make_version/slave_dht: $(MR_HOME)/slave/dht/slave.cc $(MR_HOME)/slave/dht/slave.hh $(MR_DEF) $(DHT_OBJ) $(SLAVE_DEP) $(MR_HOME)/make_version/slave
	g++ $(CCFLAGS) $(MR_HOME)/slave/dht/slave.cc -o $(MR_HOME)/bin/slave -I $(MR_HOME) $(DHT_FLAG) -lpthread 
	touch $(MR_HOME)/make_version/slave
	touch $(MR_HOME)/make_version/slave_dht

$(MR_HOME)/make_version/slave_nfs_hdfs: $(MR_HOME)/slave/slave.cc $(MR_HOME)/slave/slave.hh $(MR_DEF) $(SLAVE_DEP) $(MR_HOME)/make_version/slave
	g++ $(CCFLAGS) $(MR_HOME)/slave/slave.cc -o $(MR_HOME)/bin/slave -I $(MR_HOME) -lpthread 
	touch $(MR_HOME)/make_version/slave
	touch $(MR_HOME)/make_version/slave_nfs_hdfs


$(MR_HOME)/make_version/client_dht: $(MR_HOME)/client/dht/client.cc $(MR_HOME)/client/dht/client.hh $(MR_DEF) $(MR_HOME)/make_version/client
	g++ $(CCFLAGS) $(MR_HOME)/client/dht/client.cc -o $(MR_HOME)/bin/client -I $(MR_HOME) -lpthread
	touch $(MR_HOME)/make_version/client
	touch $(MR_HOME)/make_version/client_dht

$(MR_HOME)/make_version/client_nfs_hdfs: $(MR_HOME)/client/client.cc $(MR_HOME)/client/client.hh $(MR_DEF) $(MR_HOME)/make_version/client
	g++ $(CCFLAGS) $(MR_HOME)/client/client.cc -o $(MR_HOME)/bin/client -I $(MR_HOME) -lpthread
	touch $(MR_HOME)/make_version/client
	touch $(MR_HOME)/make_version/client_nfs_hdfs


$(MR_HOME)/make_version/mcc_nfs: $(MR_HOME)/mcc/nfs/mcc.cc $(MR_HOME)/mcc/nfs/mcc.hh $(MR_HOME)/mapreduce/nfs/mapreduce.hh $(MR_HOME)/make_version/mcc $(MR_DEF)
	g++ $(CCFLAGS) $(MR_HOME)/mcc/nfs/mcc.cc -o $(MR_HOME)/bin/mcc -I $(MR_HOME) 
	touch $(MR_HOME)/make_version/mcc
	touch $(MR_HOME)/make_version/mcc_nfs

$(MR_HOME)/make_version/mcc_hdfs: $(MR_HOME)/mcc/hdfs/mcc.cc $(MR_HOME)/mcc/hdfs/mcc.hh $(MR_HOME)/mapreduce/hdfs/mapreduce.hh $(MR_HOME)/make_version/mcc $(MR_DEF)
	g++ $(CCFLAGS) $(MR_HOME)/mcc/hdfs/mcc.cc -o $(MR_HOME)/bin/mcc -I $(MR_HOME) 
	touch $(MR_HOME)/make_version/mcc
	touch $(MR_HOME)/make_version/mcc_hdfs

$(MR_HOME)/make_version/mcc_dht: $(MR_HOME)/mcc/dht/mcc.cc $(MR_HOME)/mcc/dht/mcc.hh $(MR_HOME)/mapreduce/dht/mapreduce.hh $(MR_HOME)/make_version/mcc $(MR_DEF)
	g++ $(CCFLAGS) $(MR_HOME)/mcc/dht/mcc.cc -o $(MR_HOME)/bin/mcc -I $(MR_HOME) 
	touch $(MR_HOME)/make_version/mcc
	touch $(MR_HOME)/make_version/mcc_dht

$(MR_HOME)/bin/eclipse: $(MR_DEF) $(MR_HOME)/orthrus/launcher.cc $(MR_HOME)/orthrus/orthrus.hh $(MR_HOME)/common/utils.o $(MR_HOME)/common/hash.o $(ECLIPSE_DEP)
	g++ -g $(CCFLAGS) -std=c++11 $(MR_HOME)/orthrus/launcher.cc -I $(MR_HOME) -I $(MR_HOME)/common -I $(MR_HOME)/orthrus $(MR_HOME)/common/utils.o $(MR_HOME)/common/hash.o -o $(MR_HOME)/bin/eclipse

$(MR_HOME)/bin/cacheserver: $(MR_DEF) $(CACHESERVER_DEP)
	g++ $(CCFLAGS) $(MR_HOME)/orthrus/cacheserver.cc -I $(MR_HOME) -I $(MR_HOME)/common -o $(MR_HOME)/bin/cacheserver

#$(MR_HOME)/bin/fileserver: $(MR_HOME)/file_distributor/fileserver.cc $(MR_HOME)/file_distributor/fileserver.hh $(MR_DEF)
#	g++ $(CCFLAGS) $(MR_HOME)/file_distributor/fileserver.cc -o $(MR_HOME)/bin/fileserver -I $(MR_HOME) 


$(MR_HOME)/DHT/DHTserver.o: $(MR_HOME)/DHT/DHTserver.cc $(MR_HOME)/DHT/DHTserver.hh
	g++ $(CCFLAGS) -c $(MR_HOME)/DHT/DHTserver.cc -o $(MR_HOME)/DHT/DHTserver.o -I $(MR_HOME)/DHT/ -I $(MR_HOME)/common/ 

$(MR_HOME)/DHT/DHTclient.o: $(MR_HOME)/DHT/DHTclient.cc $(MR_HOME)/DHT/DHTclient.hh
	g++ $(CCFLAGS) -c $(MR_HOME)/DHT/DHTclient.cc -o $(MR_HOME)/DHT/DHTclient.o -I $(MR_HOME)/DHT/ -I $(MR_HOME)/common/ 


$(MR_HOME)/make_version/master:
	touch $(MR_HOME)/make_version/master

$(MR_HOME)/make_version/mcc:
	touch $(MR_HOME)/make_version/mcc

$(MR_HOME)/make_version/slave:
	touch $(MR_HOME)/make_version/slave

$(MR_HOME)/make_version/client:
	touch $(MR_HOME)/make_version/client


$(MR_HOME)/bin/fd_core: $(MR_HOME)/file_distributor/fd_core.cc
	g++ $(CCFLAGS) $(MR_HOME)/file_distributor/fd_core.cc -o $(MR_HOME)/bin/fd_core -I $(MR_HOME)/ $(MR_HOME)/common/hash.o

$(MR_HOME)/common/utils.o: $(MR_HOME)/common/utils.cc $(MR_HOME)/common/utils.hh
	g++ $(CCFLAGS) -c $(MR_HOME)/common/utils.cc -o $(MR_HOME)/common/utils.o -I $(MR_HOME)/common

$(MR_HOME)/common/hash.o: $(MR_HOME)/common/hash.cc $(MR_HOME)/common/hash.hh
	g++ $(CCFLAGS) -c $(MR_HOME)/common/hash.cc -o $(MR_HOME)/common/hash.o -I $(MR_HOME)/common

clean:
	rm -f $(MR_HOME)/bin/master
	rm -f $(MR_HOME)/bin/slave
	rm -f $(MR_HOME)/bin/client
	rm -f $(MR_HOME)/bin/mcc
	#rm -f $(MR_HOME)/bin/fileserver
	rm -f $(MR_HOME)/bin/eclipse
	rm -f $(MR_HOME)/DHT/DHTserver.o
	rm -f $(MR_HOME)/DHT/DHTclient.o
	rm -f $(MR_HOME)/make_version/*
	rm -f $(MR_HOME)/bin/fd_core
	rm -f $(MR_HOME)/bin/cacheserver
