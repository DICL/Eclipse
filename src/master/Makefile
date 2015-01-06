#
#
#
#
#
#
#
CPPFLAGS = -I../mapreduce/  -I./ -I../common
CPPFLAGS += -Wall
LDFLAGS = -lpthread
all: master

master: master.cc 
	g++ master.cc -o  master $(CPPFLAGS) $(LDFLAGS)
