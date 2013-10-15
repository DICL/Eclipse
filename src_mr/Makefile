all: $(MR_HOME)/bin/master $(MR_HOME)/bin/slave $(MR_HOME)/bin/client

$(MR_HOME)/bin/master: $(MR_HOME)/master/master.cc $(MR_HOME)/master/master.hh $(MR_HOME)/mapreduce/mapreduce.hh
	make -C master/

$(MR_HOME)/bin/slave: $(MR_HOME)/slave/slave.cc $(MR_HOME)/slave/slave.hh $(MR_HOME)/mapreduce/mapreduce.hh
	make -C slave/

$(MR_HOME)/bin/client: $(MR_HOME)/client/client.cc $(MR_HOME)/client/client.hh $(MR_HOME)/mapreduce/mapreduce.hh
	make -C client/

clean:
	rm -f $(MR_HOME)/bin/master
	rm -f $(MR_HOME)/bin/slave
	rm -f $(MR_HOME)/bin/client
