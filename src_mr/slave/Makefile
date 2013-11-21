all: $(MR_HOME)/bin/slave
$(MR_HOME)/bin/slave: slave.cc slave.hh $(MR_HOME)/mapreduce/definitions.hh
	g++ slave.cc -o $(MR_HOME)/bin/slave -lpthread -I $(MR_HOME)
