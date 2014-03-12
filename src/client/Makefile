all: $(MR_HOME)/bin/client
$(MR_HOME)/bin/client: client.cc client.hh $(MR_HOME)/mapreduce/definitions.hh
	g++ client.cc -o $(MR_HOME)/bin/client -lpthread -I $(MR_HOME) -Wall
