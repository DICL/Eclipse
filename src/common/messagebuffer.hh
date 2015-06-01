#ifndef __MESSAGEBUFFER__
#define __MESSAGEBUFFER__

#include <unistd.h>
#include <iostream>
#include <string>

using namespace std;

// message can be a null string. which means that remaining
// bytes are all null character to fit into the BUF_CUT
class messagebuffer
{
    private:
    
    public:
        string message; // should be transmitted from first one
        int fd; // fd of target object
        int remain; // remaining bytes
        
        messagebuffer();
        messagebuffer (int number);
        messagebuffer (string amessage, int number, int aremain);
        ~messagebuffer();
        void set_message (string amessage);
        void set_fd (int number);
        void set_remain (int number);
        void set_buffer (char* buf, int afd);
        void set_endbuffer (int afd);
        
        string get_message();
        int get_fd();
        int get_remain();
        bool is_end();
};

int nbwritebuf (int, char*, messagebuffer*);
int nbwritebuf (int, char*, int, messagebuffer*);

#endif
