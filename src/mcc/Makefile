all: $(MR_HOME)/bin/mcc
$(MR_HOME)/bin/mcc: mcc_nfs.cc mcc_nfs.hh $(MR_HOME)/mapreduce/mapreduce_nfs.hh
	g++ mcc.cc -o $(MR_HOME)/bin/mcc -lpthread -I $(MR_HOME) -Wall
