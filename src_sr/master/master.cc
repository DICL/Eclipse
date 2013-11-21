#include <master.hh>

bool Master::setup_network () {
 int one = 1;
 struct sockaddr_in addr = { 
	.sin_family      = AF_INET,
	.sin_port        = htons (port),
	.sin_addr.s_addr = INADDR_ANY
 };

 bzero (&(addr.sin_zero),8);

 //addr.sin_family = AF_INET;
 //addr.sin_port = htons (port);
 //addr.sin_addr.s_addr = INADDR_ANY;
 //bzero (&(addr.sin_zero),8);

 if ((sock = socket (AF_INET, SOCK_STREAM, 0)) == -1)
	log (M_ERR, "SCHEDULER", "Socket");

 if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
	log (M_ERR, "SCHEDULER", "Setsockopt");

 if (bind (sock, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == -1)
	log (M_ERR, "SCHEDULER", "Unable to bind");

 if (listen (sock, nservers + 1) == -1)
	log (M_ERR, "SCHEDULER", "Listen");

 log (M_INFO, "SCHEDULER", "Network setted up using port = %i", port);
}

//! Sample RR algorithm
int Master::select_slave (uint64_t key) {
 static int i = 0;
 return (i++ % nservers);
}

int upload (Order& o) {
 size_t size;
 int file_name = o.get_file_name ();
 char* chunk   = o.serialize (&size);

 int slave_victim = select_slave (h (data, size));

 //! Forward the data to one of the slaves
 backend [slave_victim] .send (chunk, size);
 return 0;
}
