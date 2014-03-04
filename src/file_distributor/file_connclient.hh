#ifndef __FILE_CONNCLIENT__
#define __FILE_CONNCLIENT__

#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <mapreduce/definitions.hh>

using namespace std;

class file_connclient
{
private:
	int fd;
	int writefilefd;
	int remain; // bytes remained until complete transmission
	int progress; // bytes progressed during transmission
	char buf[BUF_SIZE];
	fstream readfilestream;
	string filename;
	file_client_role role;

public:
	file_connclient(int fd);
	file_connclient(int fd, file_client_role arole, string aname);
	~file_connclient();

	int get_fd();
	void set_role(file_client_role arole);
	file_client_role get_role();
	void set_filename(string name);
	string get_filename();
	void open_readfile(string fname);
	void open_writefile(string fname);
	bool read_record(string* record);
	void write_record(string record, char* write_buf);
	void prep_send(char* source);
	void send_record();
	int get_remain();
	int get_progress();
};

file_connclient::file_connclient(int fd)
{
	this->fd = fd;
	this->writefilefd = -1;
	this->role = UNDEFINED;
	this->remain = 0;
}

file_connclient::file_connclient(int fd, file_client_role arole, string aname)
{
	this->fd = fd;
	this->filename = aname;
	this->writefilefd = -1;
	this->role = arole;
	this->remain = 0;
}

file_connclient::~file_connclient()
{
	// closing socket fd will be done exclusively
	readfilestream.close();
	close(this->writefilefd);
}

int file_connclient::get_fd()
{
	return this->fd;
}

void file_connclient::set_role(file_client_role arole)
{
	this->role = arole;
}

file_client_role file_connclient::get_role()
{
	return this->role;
}

void file_connclient::set_filename(string aname)
{
	this->filename = aname;
}

string file_connclient::get_filename()
{
	return this->filename;
}

void file_connclient::open_readfile(string fname)
{
	string fpath = DHT_PATH;
	fpath.append(fname);

	this->readfilestream.open(fpath.c_str());

	if(!this->readfilestream.is_open())
	{
		cout<<"[fileserver]File does not exist for reading"<<endl;
	}

	return;
}

void file_connclient::open_writefile(string fname)
{
	string fpath = DHT_PATH;
	fpath.append(fname);
	this->writefilefd = open(fpath.c_str(), O_APPEND|O_SYNC|O_WRONLY|O_CREAT, 0644);
	if(this->writefilefd < 0)
		cout<<"[fileserver]Opening write file failed"<<endl;
	return;
}

bool file_connclient::read_record(string* record)
{
	getline(this->readfilestream, *record);
	if(this->readfilestream.eof())
		return false;
	else
		return true;
}

void file_connclient::write_record(string record, char* write_buf)
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
		if(ret<0)
			cout<<"[fileserver]Writing to write file failed"<<endl;
	}

	// release file lock
	fcntl(this->writefilefd, F_SETLK, &ulock);

	return;
}

void file_connclient::send_record()
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

void file_connclient::prep_send(char* source) // prepare for the sending record through socket
{
	strcpy(buf, source);
	remain = BUF_CUT*(strlen(buf)/BUF_CUT+1); // same as nbwrite
	progress = 0;
}

int file_connclient::get_remain()
{
	return this->remain;
}

int file_connclient::get_progress()
{
	return this->progress;
}

#endif
