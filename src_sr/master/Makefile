INCLUDE := -I./
INCLUDE += -I./../common/
LIBDIR  := -L./../common/
LIB     := -l simring

all: clean compile
	g++ -g -Wall -o master master.o address_book.o $(INCLUDE) $(LIBDIR) $(LIB) 

compile:
	g++ -c address_book.cc -g -Wall -std=gnu++98 $(INCLUDE) -D_DEBUG
	g++ -c master.cc -g -Wall -std=gnu++98 $(INCLUDE) -D_DEBUG

clean:
	-rm *.o scheduler
