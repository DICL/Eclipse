#ifndef __PACKET_HH__
#define __PACKET_HH__

#include <metadata.hh>
#include <file.hh>

class Packet {
 public:
  Packet (File* _file, Metadata* _meta) {
   file = _file; 
   metadata = _meta;
  }

  Packet (File* _file) {
   file = _file;
   metadata = new Metadata ();
  }

  Packet (const Packet& that) {
   this->file     = that.file;
   this->metadata = that.metadata;
  }

  ~Packet () {
   if (file != NULL)     delete file;
   if (metadata != NULL) delete metadata;
  }

  Packet& set_metadata (const Metadata& _m) {
   *(this->metadata) = _m;
   return *this;
  }
 
  File* get_file () { return file; }
  Metadata* get_metadata () { return metadata; }
 
  operator File* () { return file; }
  operator Metadata* () { return metadata; }

 private:
  Metadata* metadata;
  File* file;
};

#endif
