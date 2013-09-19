LIBDIR  ?= ../../../lib
INCLUDE := -I ./../
INCLUDE += -I $(LIBDIR)/unittest++/

CXX := g++
CXXFLAGS := -g -Wall $(INCLUDE) -std=gnu++98 #-Wno-non-virtual-dtor
LDFLAGS := -L ../ -lsimring -L $(LIBDIR) -lUnitTest++ -lm

SOURCES := main.cc order_test.cc ../order.cc dht_test.cc ../dht.cc
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))

%.o : %.cc 
	$(CXX) -c $(CXXFLAGS) $^

all: $(OBJECTS) depend
	$(CXX) $(CXXFLAGS) -o ./all_test main.o order.o order_test.o dht.o dht_test.o $(LDFLAGS) 

test:
	./all_test

depend:
	@if ! [ -a $(LIBDIR)/libUnitTest++.a ]; then \
		make -C $(LIBDIR);                          \
	fi;

clean:						
	-rm *.o order_test
