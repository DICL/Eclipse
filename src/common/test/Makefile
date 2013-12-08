vpath %.cc ../

#CXX := colorgcc -lstdc++
CXX := g++
LIBDIR  ?= ../../../lib
INCLUDE := -I ./../
INCLUDE += -I $(LIBDIR)/unittest++/

CXXFLAGS := -g -Wall $(INCLUDE) -std=gnu++98 
LDFLAGS := -L $(LIBDIR) -lUnitTest++ -lm

SOURCES := main.cc ewma_test.cc metadata_test.cc file_test.cc packet_test.cc task_test.cc logger_test.cc
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))

%.o : %.cc 
	$(CXX) -c $(CXXFLAGS) $^

all: $(OBJECTS) depend
	$(CXX) $(CXXFLAGS) -o ./all_test $(OBJECTS) $(LDFLAGS) 

test: all
	./all_test

depend:
	@if ! [ $(LIBDIR)/libUnitTest++.a ]; then \
		make -C $(LIBDIR);                       \
	fi;

clean:						
	-rm *.o all_test
