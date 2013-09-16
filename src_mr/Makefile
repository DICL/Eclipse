MASTER_PATH=bin/
MASTER_SRC=master/
SLAVE_PATH=bin/
SLAVE_SRC=slave/
CLIENT_PATH=bin/
CLIENT_SRC=client/

all: $(MASTER_PATH)/master $(SLAVE_PATH)/slave $(CLIENT_PATH)/client

$(MASTER_PATH)/master: $(MASTER_SRC)/master.cc $(MASTER_SRC)/master.hh
	make -C master/

$(SLAVE_PATH)/slave: $(SLAVE_SRC)/slave.cc $(SLAVE_SRC)/slave.hh
	make -C slave/

$(CLIENT_PATH)/client: $(CLIENT_SRC)/client.cc $(CLIENT_SRC)/client.hh
	make -C client/
