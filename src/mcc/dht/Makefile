#
#
#
#
#
#
CPPFLAGS = -I ./ -I ../../ 
CPPFLAGS += -Wall
LDFLAGS = -lpthread 
all: mcc

mcc: mcc.cc
	g++ $^ -o $(BINDIR)/$@ $(CPPFLAGS) $(LDFLAGS)
	
clean:
	-rm mcc
