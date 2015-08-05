#include "file_connclient.hh"

file_connclient::file_connclient (int number)
{
    fd = number;
    thecount = NULL;
    Icachewriter = NULL;
    Itargetpeer = NULL;
    thedistributor = NULL;
    dstid = -1;
    //this->writeid = -1;
    //this->role = UNDEFINED;
    // add a null buffer
    msgbuf.push_back (new messagebuffer());
}

//file_connclient::file_connclient(int fd, file_role arole)
//{
//  this->fd = fd;
//  //this->role = arole;
//  this->writeid = -1;
//
//  // add a null buffer
//  msgbuf.push_back(new messagebuffer());
//}

file_connclient::~file_connclient()
{
    for (int i = 0; (unsigned) i < msgbuf.size(); i++)
    {
        delete msgbuf[i];
    }
    
    if (thecount != NULL)
    {
        delete thecount;
    }
    
    if (Icachewriter != NULL)
    {
        Icachewriter->complete();
        delete Icachewriter;
    }
    
    if (thedistributor != NULL)
    {
        delete thedistributor;
    }
}

int file_connclient::get_fd()
{
    return this->fd;
}

void file_connclient::set_fd (int num)
{
    this->fd = num;
}

entrywriter* file_connclient::get_Icachewriter()
{
    return Icachewriter;
}
void file_connclient::set_Icachewriter (entrywriter* thewriter)
{
    Icachewriter = thewriter;
}

filepeer* file_connclient::get_Itargetpeer()
{
    return Itargetpeer;
}

void file_connclient::set_Itargetpeer (filepeer* thepeer)
{
    Itargetpeer = thepeer;
}

int file_connclient::get_dstid()
{
    return dstid;
}

void file_connclient::set_dstid (int anumber)
{
    dstid = anumber;
}

void file_connclient::set_Icachekey (string key)
{
    Icachekey = key;
}

string file_connclient::get_Icachekey()
{
    return Icachekey;
}

//void file_connclient::set_role(file_role arole)
//{
//  this->role = arole;
//}

//file_role file_connclient::get_role()
//{
//  return this->role;
//}

//int file_connclient::get_writeid()
//{
//  return writeid;
//}

//void file_connclient::set_writeid(int num)
//{
//  writeid = num;
//}

