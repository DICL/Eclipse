########################
# Policies List        #
# ===================  #
# - DATA_MIGRATION     #
# - LRU POP POLICY     #
# - PUSH POLICY        #
# - BDEMA              #
# - ROUND_ROBIN        #
#                      #
########################

CXX = gcc
MAKE = make
AR = ar

CXXFLAGS  = -Wall -g -std=gnu++98 -rdynamic
INCLUDE   = -I./lib/ -I./src/common/ -I ./lib/unittest-cpp/ -L./lib/ 
BINLIB    = -lstdc++ -lsimring
LIBDIR   := $(realpath ./lib/)

#Experiments parameters
OPTIONS  = -D__STDC_FORMAT_MACROS
OPTIONS += -DALPHA=0.03f
OPTIONS += -DCACHESIZE=1000
OPTIONS += -DDATA_MIGRATION

POLICY = -DDATA_MIGRATION

export POLICY CXX CXXFLAGS MAKE AR OPTIONS INCLUDE BINLIB LIBDIR
.PHONY: lib dist node docs src

all: lib src
	@echo -e "\e[31m*************DONE**************\e[0m"

lib:
	@echo building a static library!!
	-$(MAKE) -C lib/ -j16

src:
	-$(MAKE) -C src/

clean:
	-$(MAKE) -C lib/ clean
	-$(MAKE) -C src/ clean

test: 
	-$(MAKE) -C src/ test

tags:
	-ctags -R --c++-kinds=+p --fields=+iaS --extra=+q -o .tags .

dist: clean
	tar -cvzf MRR_`date +"%d-%m-%y"`.tar.gz ./*

docs:
	cd docs; doxygen
