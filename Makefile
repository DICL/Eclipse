########################

CXX = gcc
MAKE = make
AR = ar

CXXFLAGS  = -Wall -g -std=gnu++98 -rdynamic
INCLUDE   = -I./lib/ -I./src/common/ -I ./lib/unittest-cpp/ -L./lib/ 
BINLIB    = -lstdc++ -lsimring
LIBDIR   := $(realpath ./lib/)
SRC       = $(shell find src/ -name "*.cc" -o -name "*.hh")
ARTISTIC_STYLE_OPTIONS = -A1 -s2 -C -E --unpad-paren --pad-paren-out --pad-header \
						 --break-blocks=all --convert-tabs --pad-oper -n

export CXX CXXFLAGS MAKE AR OPTIONS INCLUDE BINLIB LIBDIR ARTISTIC_STYLE_OPTIONS
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

style:
	astyle $(ARTISTIC_STYLE_OPTIONS) $(SRC)

tags:
	-ctags -R --c++-kinds=+p --fields=+iaS --extra=+q -o .tags .

dist: clean
	tar -cvzf MRR_`date +"%d-%m-%y"`.tar.gz ./*

docs:
	cd docs; doxygen
