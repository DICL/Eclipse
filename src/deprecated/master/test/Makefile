vpath %.cc ../

LIBDIR  ?= ../../../lib
INCLUDE := -I ./../
INCLUDE += -I ./../../common
INCLUDE += -I $(LIBDIR)
INCLUDE += -I $(LIBDIR)/unittest++/

CXX := colorgcc -lstdc++
CXXFLAGS := -g -Wall $(INCLUDE) -std=gnu++98 
LDFLAGS := -L ../ -lsimring -L $(LIBDIR) -lUnitTest++ -lm

SOURCES := main.cc address_book_page.cc address_book_page_test.cc
#SOURCES := main.cc master.cc master_test.cc address_book_page.cc address_book_page_test.cc
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
