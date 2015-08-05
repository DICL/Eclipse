#include "filebridge.hh"

filebridge::filebridge (int anid)
{
    id = anid;
    dstid = -1;
    //writefilefd = -1;
    thecount = NULL;
    dstpeer = NULL;
    dstclient = NULL;
    srcentryreader = NULL;
    dstentrywriter = NULL;
    writebuffer = NULL;
    theidistributor = NULL;
    //keybuffer.configure_initial("Ikey\n");

    Settings setted;
    setted.load();
    dht_path = setted.get<string>("path.scratch");
}

filebridge::~filebridge()
{
    // clear up all things
    if (thecount != NULL)
    {
        delete thecount;
    }
    
    readfilestream.close();
    
    if (writebuffer != NULL)
    {
        delete writebuffer;
    }
    
    //close(this->writefilefd);
}

void filebridge::set_entryreader (entryreader* areader)
{
    srcentryreader = areader;
}

void filebridge::set_entrywriter (entrywriter* awriter)
{
    dstentrywriter = awriter;
}

void filebridge::set_dstpeer (filepeer* apeer)
{
    this->dstpeer = apeer;
}

void filebridge::set_distributor (idistributor* thedistributor)
{
    theidistributor = thedistributor;
}

idistributor* filebridge::get_distributor()
{
    return theidistributor;
}

filepeer* filebridge::get_dstpeer()
{
    return dstpeer;
}

entryreader* filebridge::get_entryreader()
{
    return srcentryreader;
}

entrywriter* filebridge::get_entrywriter()
{
    return dstentrywriter;
}

void filebridge::open_readfile (string fname)
{
    string fpath = dht_path;
    fpath += "/"; // Vicente (this solves critical error)
    fpath.append (fname);
    this->readfilestream.open (fpath.c_str());
    
    if (!this->readfilestream.is_open())
    {
        cout << "[filebridge]File does not exist for reading: " << fname << endl;
    }
    
    return;
}

bool filebridge::read_record (string& record)     // reads next record
{
    getline (this->readfilestream, record);
    
    if (this->readfilestream.eof())
    {
        return false;
    }
    else
    {
        return true;
    }
}

file_connclient* filebridge::get_dstclient()
{
    return this->dstclient;
}

void filebridge::set_srctype (bridgetype atype)
{
    this->srctype = atype;
}

void filebridge::set_dsttype (bridgetype atype)
{
    this->dsttype = atype;
}

bridgetype filebridge::get_srctype()
{
    return this->srctype;
}

bridgetype filebridge::get_dsttype()
{
    return this->dsttype;
}

int filebridge::get_id()
{
    return id;
}

int filebridge::get_dstid()
{
    return dstid;
}

void filebridge::set_id (int num)
{
    id = num;
}

void filebridge::set_dstid (int num)
{
    dstid = num;
}

//file_role filebridge::get_role()
//{
//  return this->dstclient->get_role();
//}

void filebridge::set_dstclient (file_connclient* aclient)
{
    this->dstclient = aclient;
}
void filebridge::set_Icachekey (string key)
{
    Icachekey = key;
}

string filebridge::get_Icachekey()
{
    return Icachekey;
}

void filebridge::set_jobdirpath (string path)
{
    jobdirpath = path;
}

string filebridge::get_jobdirpath()
{
    return jobdirpath;
}

