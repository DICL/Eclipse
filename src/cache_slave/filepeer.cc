#include "filepeer.hh"

filepeer::filepeer (int afd, string anaddress)
{
    this->fd = afd;
    this->address = anaddress;
    //writebuffer.configure_initial("write\n");
    //writebuffer.set_msgbuf(&msgbuf);
    // add a null message buffer
    msgbuf.push_back (new messagebuffer());
}


filepeer::~filepeer()
{
    for (int i = 0; (unsigned) i < msgbuf.size(); i++)
    {
        if (msgbuf[i] != NULL)
        {
            delete msgbuf[i];
        }
    }
}

int filepeer::get_fd()
{
    return this->fd;
}

string filepeer::get_address()
{
    return this->address;
}

void filepeer::set_fd (int num)
{
    this->fd = num;
}

void filepeer::set_address (string astring)
{
    this->address = astring;
}

