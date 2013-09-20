CXXFLAGS += -I.
sources := $(wildcard *.cc) 
objects := $(patsubst %.cc, %.o, $(sources))
.SUFFIXES: .o .cc

.cc.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(OPTIONS)

all: $(objects) 
	@echo -ne "\e[33mCreating simring library...\e[0m"
	@-rm libsimring.a
	@$(AR) -cr libsimring.a $(objects)
	@echo -ne "\e[33mDONE\e[0m\n"
	@-rm $(objects)

$(objects) : $(sources)

test:
	make -C test/ test 

clean:
	-rm *.o *.a
