#ifndef __CACHECLIENT__
#define __CACHECLIENT__

#include <iostream>

using namespace std;

class cacheclient
{
    private:
        int rank;
        
    public:
        cacheclient (int rank);
        int get_fd();
        void set_fd (int num);
        string get_address();
        void set_address (string anaddress);
};

cacheclient::cacheclient (int rank)
{
    this->rank = rank;
}

int cacheclient::get_rank()
{
    return rank;
}

void cacheclient::set_rank (int rank)
{
    this->rank = rank;
}

#endif
