#ifndef __MESSAGEBUFFER__
#define __MESSAGEBUFFER__

#include <unistd.h>
#include <iostream>

using namespace std;

// message can be a null string. which means that remaining
// bytes are all null character to fit into the BUF_CUT
class messagebuffer {
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

messagebuffer::messagebuffer() {
  // constructor of null message
  fd = -1;
  remain = 0;
}

messagebuffer::messagebuffer (int afd) {   // same as end buffer
  fd = afd;
  remain = 0;
}

messagebuffer::messagebuffer (string amessage, int afd, int aremain) {
  this->message = amessage;
  this->fd = afd;
  this->remain = aremain;
}

messagebuffer::~messagebuffer() {
  // do nothing as default
}

void messagebuffer::set_buffer (char* buf, int afd) {
  this->fd = afd;
  this->remain = BUF_CUT * (strlen (buf) / BUF_CUT + 1);
  this->message = buf;
}

void messagebuffer::set_endbuffer (int afd) {
  this->fd = afd;
  this->remain = 0;
}

void messagebuffer::set_message (string amessage) {
  this->message = amessage;
}

void messagebuffer::set_fd (int afd) {
  this->fd = afd;
}

void messagebuffer::set_remain (int number) {
  this->remain = number;
}

string messagebuffer::get_message() {
  return this->message;
}

int messagebuffer::get_fd() {
  return this->fd;
}

int messagebuffer::get_remain() {
  return this->remain;
}

bool messagebuffer::is_end() { // true if close(fd) is needed
  if (fd > 0 && remain == 0)
    return true;
    
  else
    return false;
}


// memset is needed explicitly before the function call
// when positive value is returned, you can still use the null buffer
// when zero value is returned, transmission is partially done. you should add another null bufferpointer
// when negative value is returned, whole message is not transmitted. you should add another null bufferpointer
int nbwritebuf (int fd, char* buf, messagebuffer* buffer) {
  // bufferpointer <- a null buffer as an input
//cout<<"message: "<<buf<<endl;
  int written_bytes;
  int writing_bytes = BUF_CUT * (strlen (buf) / BUF_CUT + 1);
  
  if (writing_bytes == 0)
    cout << "check writing_bytes in the nbwritebuf" << endl;
    
  written_bytes = write (fd, buf, writing_bytes);
  
//cout<<"writing bytes: "<<writing_bytes<<endl;
//cout<<"written bytes: "<<written_bytes<<endl<<endl;

  if (written_bytes == writing_bytes) {
    return written_bytes;
    
  } else if (written_bytes > 0) {
    string message = buf + written_bytes;
    
    buffer->set_fd (fd);
    buffer->set_message (message);
    buffer->set_remain (writing_bytes - written_bytes);
    
    return 0;
    
  } else { // -1 returned, totally failed
    string message = buf;
    
    buffer->set_fd (fd);
    buffer->set_message (message);
    buffer->set_remain (writing_bytes);
    
    return -1;
  }
}

int nbwritebuf (int fd, char* buf, int writing_bytes, messagebuffer* buffer) {
  // bufferpointer <- a null buffer as an input
//cout<<"message: "<<buf<<endl;
  int written_bytes;
  written_bytes = write (fd, buf, writing_bytes);
  
//cout<<"writing bytes: "<<writing_bytes<<endl;
//cout<<"written bytes: "<<written_bytes<<endl<<endl;


  if (written_bytes == writing_bytes) {
    return 1;
    
  } else if (written_bytes > 0) {
    string message = buf + written_bytes;
    
    buffer->set_fd (fd);
    buffer->set_message (message);
    buffer->set_remain (writing_bytes - written_bytes);
    
    return 0;
    
  } else { // -1 returned, totally failed
    string message = buf;
    
    buffer->set_fd (fd);
    buffer->set_message (message);
    buffer->set_remain (writing_bytes);
    
    return -1;
  }
}

#endif
