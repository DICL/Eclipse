INCLUDE += -I./../../lib/ -I./
INCLUDE += -I./../common/
LIBDIR  += -L./../common/
BINLIB  += -lpthread

all: clean compile
	$(CXX) $(CXXFLAGS) -g -Wall -o node node.o main.o  \
	$(INCLUDE) $(LIBDIR) $(BINLIB) \
	$(OPTIONS)

compile:
	$(CXX) $(CXXFLAGS) -c node.cc $(INCLUDE) \
	$(OPTIONS)
	$(CXX) $(CXXFLAGS) -c main.cc $(INCLUDE) \
	$(OPTIONS)

clean:
	-rm *.o node
