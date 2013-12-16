all: $(MR_HOME)/bin/slave
$(MR_HOME)/bin/slave: slave.cc slave.hh slave_job.hh slave_task.hh $(MR_HOME)/mapreduce/definitions.hh
	g++ slave.cc -o $(MR_HOME)/bin/slave -lpthread -I $(MR_HOME) -Wall
