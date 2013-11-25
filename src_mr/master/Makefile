all: $(MR_HOME)/bin/master
$(MR_HOME)/bin/master: master.cc master.hh $(MR_HOME)/mapreduce/definitions.hh $(MR_HOME)/mapreduce/task.hh $(MR_HOME)/mapreduce/job.hh connslave.hh connclient.hh dec_connclient.hh
	g++ master.cc -o $(MR_HOME)/bin/master -lpthread -I $(MR_HOME)
