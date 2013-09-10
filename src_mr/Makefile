MASTER_PATH=bin/
MASTER_SRC=master/
SLAVE_PATH=bin/
SLAVE_SRC=slave/

all: $(MASTER_PATH)/master $(SLAVE_PATH)/slave

$(MASTER_PATH)/master: $(MASTER_SRC)/master.cc $(MASTER_SRC)/master.hh
	make -C master/

$(SLAVE_PATH)/slave: $(SLAVE_SRC)/slave.cc $(SLAVE_SRC)/slave.hh
	make -C slave/
