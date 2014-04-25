#ifndef __FILEBRIDGE__
#define __FILEBRIDGE__

#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <mapreduce/definitions.hh>
#include "filepeer.hh"
#include "file_connclient.hh"

using namespace std;

class filebridge
{
	private:
		int id; // this id will be sent to peer to link this filebridge
		int dstid; // the bridge id of remote peer, -1 as default, positive value when the dsttype is PEER
		int remain; // bytes remained until complete transmission
		int progress; // bytes progressed during transmission
		int writefilefd;
		char buf[BUF_SIZE];
		string dataname; // the key of the data which is used as input of hash function
		bridgetype srctype; // PEER, DISK, CACHE or CLIENT
		bridgetype dsttype; // PEER, DISK, CACHE or CLIENT
		datatype dtype; // RAW, INTERMEDIATE, OUTPUT
		filepeer* dstpeer; // destination peer
		file_connclient* dstclient; // destination client. should be set in the constructor either to an real object or NULL pointer
		fstream readfilestream;
		string filename; // [jobindex(if it is intermediate)] + [dataname]

		string cachebuffer;

	public:
		filebridge(int anid);
		~filebridge();

		void set_dstpeer(filepeer* apeer);
		void set_role(file_role arole);
		void set_dataname(string aname);
		void set_filename(string aname);
		void set_dtype(datatype atype);
		void set_remain(int num);
		void set_progress(int num);
		void set_srctype(bridgetype atype);
		void set_dsttype(bridgetype atype);
		void set_id(int num);
		void set_dstid(int num);
		void set_dstclient(file_connclient* aclient);

		int get_id();
		int get_dstid();
		int get_remain();
		int get_progress();
		filepeer* get_dstpeer();
		datatype get_dtype();
		file_role get_role();
		string get_dataname();
		string get_filename();
		file_connclient* get_dstclient();
		bridgetype get_srctype();
		bridgetype get_dsttype();

		void open_readfile(string fname);
		void open_writefile(string fname);
		void write_record(string record, char* write_buf);
		bool read_record(string * record);
		// void send_record();
		// void prep_send(char* source);
};

filebridge::filebridge(int anid)
{
	id = anid;
	dstid = -1;
	remain = 0;
	progress = 0;
	writefilefd = -1;
	dstpeer = NULL;
	dstclient = NULL;
}

filebridge::~filebridge()
{
	// clear up all things
	readfilestream.close();
	close(this->writefilefd);
}

void filebridge::set_dstpeer(filepeer* apeer)
{
	this->dstpeer = apeer;
}

void filebridge::set_role(file_role arole)
{
	if(this->dstclient == NULL)
	{
		cout<<"[filebridge]The destination client of a filebridge is not set and modifying the member client tried"<<endl;
		return;
	}
	this->dstclient->set_role(arole);
}

void filebridge::set_dataname(string aname)
{
	this->dataname = aname;
}

void filebridge::set_dtype(datatype atype)
{
	this->dtype = atype;
}

filepeer* filebridge::get_dstpeer()
{
	return dstpeer;
}

string filebridge::get_dataname()
{
	return dataname;
}

datatype filebridge::get_dtype()
{
	return dtype;
}

int filebridge::get_remain()
{
	return this->remain;
}

int filebridge::get_progress()
{
	return this->progress;
}

void filebridge::set_remain(int num)
{
	this->remain = num;
}

void filebridge::set_progress(int num)
{
	this->progress = num;
}

void filebridge::set_filename(string aname)
{
	this->filename = aname;
}

string filebridge::get_filename()
{
	return this->filename;
}

void filebridge::open_readfile(string fname)
{
	string fpath = DHT_PATH;
	fpath.append(fname);

	this->readfilestream.open(fpath.c_str());


	if(!this->readfilestream.is_open())
		cout<<"[filebridge]File does not exist for reading"<<endl;

	return;
}

void filebridge::open_writefile(string fname)
{
	string fpath = DHT_PATH;
	fpath.append(fname);
	this->writefilefd = open(fpath.c_str(), O_APPEND|O_SYNC|O_WRONLY|O_CREAT, 0644);
	if(this->writefilefd < 0)
		cout<<"filebridge]Opening write file failed"<<endl;
	return;
}

void filebridge::write_record(string record, char* write_buf)
{
	struct flock alock;
	struct flock ulock;

	// set lock
	alock.l_type = F_WRLCK;
	alock.l_start = 0;
	alock.l_whence = SEEK_SET;
	alock.l_len = 0;

	// set unlock
	ulock.l_type = F_UNLCK;
	ulock.l_start = 0;
	ulock.l_whence = SEEK_SET;
	ulock.l_len = 0;


	// acquire file lock
	fcntl(this->writefilefd, F_SETLKW, &alock);

	// critical section
	{
		int ret;
		record.append("\n");
		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, record.c_str());
		ret = write(this->writefilefd, write_buf, record.length());

		if(ret < 0)
			cout<<"[filebridge]Writing to write file failed"<<endl;
	}

	// release file lock
	fcntl(this->writefilefd, F_SETLK, &ulock);
}

bool filebridge::read_record(string * record)
{
	getline(this->readfilestream, *record);
	if(this->readfilestream.eof())
		return false;
	else
		return true;
}

/*
   void filebridge::send_record()
   {
   int written_bytes;
   written_bytes = write(this->fd, buf+progress, remain);

   if(written_bytes > 0)
   {
   progress += written_bytes;
   remain -= written_bytes;
   }
   return;
   }

   void filebridge::prep_send(char* source)
   {
   memset(buf, 0, BUF_SIZE);
   strcpy(buf, source);
   remain = BUF_CUT*(strlen(buf)/BUF_CUT+1); // same as nbwrite
   progress = 0;
   }
   */

file_connclient* filebridge::get_dstclient()
{
	return this->dstclient;
}

void filebridge::set_srctype(bridgetype atype)
{
	this->srctype = atype;
}

void filebridge::set_dsttype(bridgetype atype)
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

void filebridge::set_id(int num)
{
	id = num;
}

void filebridge::set_dstid(int num)
{
	dstid = num;
}

file_role filebridge::get_role()
{
	return this->dstclient->get_role();
}

void filebridge::set_dstclient(file_connclient* aclient)
{
	this->dstclient = aclient;
}

#endif
