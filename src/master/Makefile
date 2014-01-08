all: $(MR_HOME)/bin/master
$(MR_HOME)/bin/master: master.cc master.hh $(MR_HOME)/mapreduce/definitions.hh connslave.hh connclient.hh master_job.hh master_task.hh 
	g++ master.cc -o $(MR_HOME)/bin/master -lpthread -I $(MR_HOME) -Wall
