#define _DEBUG

#include <node.hh>
#include <simring.hh>
#include <stdio.h>
#include <string.h>

packet packet_vector [] = {packet (1), packet (2), packet (3), packet (4), packet (5)}; 
int c = 0;

ssize_t recv_mock (int fd, void* buff, size_t s, int flags) {
	puts ("=======RECV_MOCK=======");
	memcpy (buff, (packet_vector + c) , sizeof (packet));
  c = (c + 1) % 5;
	printf ("FD: %i\nbuff: %i\nsize: %i\nflags: %i\n",
           fd, c, (int)s, flags);

	return static_cast<ssize_t> (s);
}

ssize_t send_mock (int fd, const void* buff, size_t s, int flags) {
	puts ("=======SEND_MOCK=======");
	printf ("FD: %i\nbuff: %s\nsize: %i\nflags: %i\n",
           fd, (char*)buff, (int)s, flags);

	return static_cast<ssize_t> (s);
}
