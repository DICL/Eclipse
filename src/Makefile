




all: DHT

DHT:
	$(MAKE) -C client/dht/
	$(MAKE) -C master/dht/
	$(MAKE) -C slave/dht/
	$(MAKE) -C mcc/dht/
	$(MAKE) -C file_distributor/
	$(MAKE) -C orthrus/

clean:
	-$(MAKE) -C client/dht/ clean
	-$(MAKE) -C master/dht/ clean
	-$(MAKE) -C slave/dht/  clean
	-$(MAKE) -C mcc/dht/    clean 
	-$(MAKE) -C file_distributor/ clean 
	-$(MAKE) -C orthrus/ clean
