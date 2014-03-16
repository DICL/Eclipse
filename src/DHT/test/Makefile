vpath %.cc ../

#CXX := colorgcc -lstdc++
CXX := g++
LIBDIR  ?= ../../../lib
LIBDIR2 := ../../common
INCLUDE := -I ./../
INCLUDE += -I ./../../common
INCLUDE += -I $(LIBDIR)/unittest++/

<<<<<<< HEAD
CXXFLAGS := -g -Wall $(INCLUDE) -std=gnu++98 -fopenmp # -D_DEBUG
LDFLAGS := -L $(LIBDIR) -L $(LIBDIR2) -L $(LIBDIR)/unittest++ -lUnitTest++ -lm -lpthread -lmrr
=======
CXXFLAGS := -g -Wall $(INCLUDE) -std=gnu++11 -fopenmp # -D_DEBUG
LDFLAGS := -L $(LIBDIR) -L $(LIBDIR2) -lUnitTest++ -lm -lpthread -lmrr
>>>>>>> 533d4daa751a93b458d68e70af38c8c3864a4c53

SOURCES := DHTserver.cc DHTclient.cc DHTserver_test.cc DHTclient_test.cc integration.cc main.cc 
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))

%.o : %.cc 
	$(CXX) -c $(CXXFLAGS) $^

all: unit_test _clean  

unit_test: $(OBJECTS) depend
	$(CXX) $(CXXFLAGS) -o ./all_test $(OBJECTS) $(LDFLAGS) 

test: all
	./all_test

depend:
	@if ! [ $(LIBDIR)/libUnitTest++.a ]; then \
		make -C $(LIBDIR);                       \
	fi;

_clean:
	-rm *.o

clean:						
	-rm *.o ./all_test
