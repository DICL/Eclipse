vpath %.cc ../

LIBDIR  ?= ../../../lib
INCLUDE := -I ./../
INCLUDE += -I $(LIBDIR)/unittest++/

CXXFLAGS := -g -Wall $(INCLUDE) -std=gnu++98 
LDFLAGS := -L ../ -lsimring -L $(LIBDIR) -lUnitTest++ -lm

SOURCES := main.cc order_test.cc dht_test.cc ewma_test.cc order.cc dht.cc
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))

%.o : %.cc 
	$(CXX) -c $(CXXFLAGS) $^

all: $(OBJECTS) depend
	$(CXX) $(CXXFLAGS) -o ./all_test $(OBJECTS) $(LDFLAGS) 

test: all
	./all_test

depend:
	@if ! [ -a $(LIBDIR)/libUnitTest++.a ]; then \
		make -C $(LIBDIR);                          \
	fi;

clean:						
	-rm *.o all_test
