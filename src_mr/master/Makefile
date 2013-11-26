all: $(MR_HOME)/bin/master
$(MR_HOME)/bin/master: master.cc master.hh $(MR_HOME)/mapreduce/definitions.hh master_task.hh connslave.hh connclient.hh dec_connclient.hh master_job.hh
	g++ master.cc -o $(MR_HOME)/bin/master -lpthread -I $(MR_HOME)
