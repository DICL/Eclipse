#
#g++ -Wall -g /home/vicente/Eclipse/src/file_distributor/mrcat_core.cc -o /home/vicente/Eclipse/src/bin/mrcat_core -I /home/vicente/Eclipse/src/ /home/vicente/Eclipse/src/common/hash.o
#g++ -Wall -g /home/vicente/Eclipse/src/file_distributor/mrrm_core.cc -o /home/vicente/Eclipse/src/bin/mrrm_core -I /home/vicente/Eclipse/src/ /home/vicente/Eclipse/src/common/hash.o
#g++ -Wall -g /home/vicente/Eclipse/src/file_distributor/fd_core.cc -o /home/vicente/Eclipse/src/bin/fd_core -I /home/vicente/Eclipse/src/ /home/vicente/Eclipse/src/common/hash.o
#
#
#
#
#
CPPFLAGS = -I ./ -I ../ -I ../common
CPPFLAGS += -Wall
LDFLAGS = -lpthread 
BINDIR ?= ./
SOURCES := mrcat_core.cc mrrm_core.cc fd_core.cc 
TARGETS = $(SOURCES:.cc=)
	
all: $(TARGETS)

mrcat_core: mrcat_core.cc ../common/hash.cc
	g++ $^ -o $(BINDIR)/$@ $(CPPFLAGS) $(LDFLAGS)

mrrm_core: mrrm_core.cc ../common/hash.cc
	g++ $^ -o $(BINDIR)/$@ $(CPPFLAGS) $(LDFLAGS)

fd_core: fd_core.cc ../common/hash.cc
	g++ $^ -o $(BINDIR)/$@ $(CPPFLAGS) $(LDFLAGS)

clean:
	-rm $(TARGETS)
