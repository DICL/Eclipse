#
#
#
#
#
#
CPPFLAGS = -I ./ -I ../../ 
CPPFLAGS += -Wall
LDFLAGS = -lpthread 
all: client

client: client.cc
	g++ $^ -o $(BINDIR)/$@ $(CPPFLAGS) $(LDFLAGS)

clean:
	-rm client
