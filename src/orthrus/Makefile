
#g++ -g -Wall -g /home/vicente/Eclipse/src/orthrus/launcher.cc -I /home/vicente/Eclipse/src -I /home/vicente/Eclipse/src/common -I /home/vicente/Eclipse/src/orthrus /home/vicente/Eclipse/src/common/hash.o -o /home/vicente/Eclipse/src/bin/eclipse
#g++ -Wall -g /home/vicente/Eclipse/src/orthrus/cacheserver.cc -I /home/vicente/Eclipse/src -I /home/vicente/Eclipse/src/common -o /home/vicente/Eclipse/src/bin/cacheserver
#
#
#
#
#
CPPFLAGS = -I ./ -I ../common -I ../
CPPFLAGS += -Wall
LDFLAGS = -L ../../common/ -lpthread 
all: eclipse cacheserver

eclipse: launcher.cc ../common/hash.o
	g++ $^ -o $@ $(CPPFLAGS) $(LDFLAGS)

cacheserver: cacheserver.cc
	g++ $^ -o $@ $(CPPFLAGS) $(LDFLAGS)

clean:
	-rm eclipse cacheserver
