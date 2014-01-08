all: $(MR_HOME)/bin/mcc
$(MR_HOME)/bin/mcc: mcc.cc mcc.hh $(MR_HOME)/mapreduce/mapreduce.hh
	g++ mcc.cc -o $(MR_HOME)/bin/mcc -lpthread -I $(MR_HOME) -Wall
