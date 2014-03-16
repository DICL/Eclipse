vpath %.cc ../
###################################################################
#Parameters
SOURCES := main.cc address_book.cc address_book_page.cc 
SOURCES += address_book_page_test.cc scheduler.cc scheduler_test.cc
SOURCES += #add here the file .cc and the unit test file you want to compile

LIBDIR  ?= ../../../lib
COMDIR  ?= ./../../common
###################################################################

INCLUDE := -I ./../
INCLUDE += -I $(COMDIR)
INCLUDE += -I $(LIBDIR)
INCLUDE += -I $(LIBDIR)/unittest++/

CXX := g++ -std=gnu++98 
CXXFLAGS := -g -Wall $(INCLUDE) 
LDFLAGS := -L $(COMDIR) -lmrr -L $(LIBDIR) -lUnitTest++ -lm

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
