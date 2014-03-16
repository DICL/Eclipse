#ifndef __PACKET_HH__
#define __PACKET_HH__

#include <metadata.hh>
#include <task.hh>

class Packet {
 public:
  Packet (Task* _task, Metadata* _meta) {
   task = _task; 
   metadata = _meta;
  }

  Packet (Task* _task) {
   task = _task;
   metadata = new Metadata ();
  }

  Packet (const Packet& that) {
   this->task     = that.task;
   this->metadata = that.metadata;
  }

  ~Packet () {
   if (task != NULL)     delete task;
   if (metadata != NULL) delete metadata;
  }

  size_t get_size () {
   return metadata->get_size () + task->get_size ();
  }

  Packet& set_metadata (const Metadata& _m) {
   *(this->metadata) = _m;
   return *this;
  }
 
  Task* get_task () { return task; }
  Metadata* get_metadata () { return metadata; }
 
  operator Task* () { return task; }
  operator Metadata* () { return metadata; }

 private:
  Metadata* metadata;
  Task* task;
};

#endif
