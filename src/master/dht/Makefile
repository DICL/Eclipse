#
#
#
#
#
#
CPPFLAGS = -I../mapreduce/  -I./ -I../../common -I../../ 
CPPFLAGS += -Wall
LDFLAGS = -lpthread 
all: master

master: master.cc  ../../common/hash.cc
	g++ $(CPPFLAGS) $^ -o $(BINDIR)/$@ $(CPPFLAGS) $(LDFLAGS)

clean:
	-rm master
