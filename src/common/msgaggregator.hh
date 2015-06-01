#ifndef __MSGAGGREGATOR__
#define __MSGAGGREGATOR__

#include <iostream>
#include <errno.h>
#include <string.h>
#include <vector>
#include <unistd.h>
#include <stdlib.h>

#include "ecfs.hh"
#include "messagebuffer.hh"
#include "dataentry.hh"

using namespace std;

class msgaggregator
{
    private:
        int fd;
        int pos;
        char message[BUF_SIZE];
        string initial;
        
    public:
        vector<messagebuffer*>* msgbuf;
        entrywriter* dwriter;
        
        // public functions
        msgaggregator(); // constructor
        msgaggregator (int number);   // constructor
        int get_available(); // remaining capacity, not the full capacity
        bool add_record (string& record);   // <- automatically flushed??
        bool add_record (char*& record);   // <- automatically flushed??
        void flush(); // flush and re-initialize the message
        
        void configure_initial (string record);
        void set_initial();
        void set_msgbuf (vector<messagebuffer*>* target);
        void set_dwriter (entrywriter* awriter);
        void set_fd (int num);
        int get_fd();
        char* get_buf();
};

#endif
