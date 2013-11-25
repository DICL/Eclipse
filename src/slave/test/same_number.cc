#define _DEBUG

#include <node.hh>
#include <stdio.h>
#include <string.h>

int recv_mock (int fd, void* buff, size_t s, int flags) {
	puts ("=======RECV_MOCK=======");
	printf ("FD: %i\nbuff: %s\nsize: %z\nflags: %i\n",
           fd, buff, s, flags);
	int vect [sizeof(packet)];
	memcpy (buff, vect, sizeof (packet));

	return static_cast<int> (s);
}

int send_mock (int fd, void* buff, size_t s, int flags) {
	puts ("=======SEND_MOCK=======");
	printf ("FD: %i\nbuff: %s\nsize: %z\nflags: %i\n",
           fd, buff, s, flags);

	return static_cast<int> (s);
}

