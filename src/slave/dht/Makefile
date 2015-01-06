#
#
#
#
#
#
CPPFLAGS = -I ./ -I ../../common -I ../../ 
CPPFLAGS += -Wall
LDFLAGS = -L ../../common/ -lpthread 
all: slave

slave: slave.cc
	g++ $^ -o $(BINDIR)/$@ $(CPPFLAGS) $(LDFLAGS)

clean:
	-rm slave
