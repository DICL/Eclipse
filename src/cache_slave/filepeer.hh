#ifndef __FILEPEER__
#define __FILEPEER__

#include <iostream>
#include <vector>
//##include "../common/msgaggregator.hh"
#include "../common/messagebuffer.hh"

using namespace std;

class filepeer
{
    private:
        int fd;
        string address;
        
    public:
        vector<messagebuffer*> msgbuf;
        //msgaggregator writebuffer;
        
        filepeer (int afd, string anaddress);
        ~filepeer();
        int get_fd();
        void set_fd (int num);
        string get_address();
        void set_address (string astring);
};

#endif
