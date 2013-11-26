all: $(MR_HOME)/bin/master $(MR_HOME)/bin/slave $(MR_HOME)/bin/client $(MR_HOME)/bin/mcc

$(MR_HOME)/bin/master: $(MR_HOME)/master/master.cc $(MR_HOME)/master/master.hh $(MR_HOME)/mapreduce/definitions.hh $(MR_HOME)/mapreduce/job.hh $(MR_HOME)/mapreduce/task.hh $(MR_HOME)/master/connslave.hh $(MR_HOME)/master/connclient.hh $(MR_HOME)/master/dec_connclient.hh
	make -C master/

$(MR_HOME)/bin/slave: $(MR_HOME)/slave/slave.cc $(MR_HOME)/slave/slave.hh $(MR_HOME)/mapreduce/definitions.hh
	make -C slave/

$(MR_HOME)/bin/client: $(MR_HOME)/client/client.cc $(MR_HOME)/client/client.hh $(MR_HOME)/mapreduce/definitions.hh
	make -C client/

$(MR_HOME)/bin/mcc: $(MR_HOME)/mcc/mcc.cc $(MR_HOME)/mcc/mcc.hh $(MR_HOME)/mapreduce/definitions.hh
	make -C mcc/

clean:
	rm -f $(MR_HOME)/bin/master
	rm -f $(MR_HOME)/bin/slave
	rm -f $(MR_HOME)/bin/client
	rm -f $(MR_HOME)/bin/mcc
