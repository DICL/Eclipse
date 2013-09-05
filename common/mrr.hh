//Public interface between SR and MR
//
//


#ifndef _DEBUG
#include <sr/master.hh>
#include <sr/utils.hh>
#include <sr/packets.hh>
#else

class Order {
 private:
  size_t DISK_PAGE_SIZE;
  char file_name [128];
  list<uint8_t*> list_chunk; //! Splited in 4KiB chuncks

 public:
  Order (char*, void*, size_t);
  Order (char*);

  void deserialize (char * chunk) {
    uint32_t size;
    memcpy (size, chunk[0], 4);
    memcpy (file_name, chunk[4], 128);

    for (int i = 0; i < size; i++) {
     uint8_t* chunky = new uint8_t [DISK_PAGE_SIZE];
     memcpy (chunky, chunk[132 + (i * DISK_PAGE_SIZE)], DISK_PAGE_SIZE);
     list_chunk.insert (chunky);
    }  
  }

  uint8_t* serialize (size_t* size = NULL) {
   size_t SIZE_TO_SEND = (size_t) (list_chunk.size () * DISK_PAGE_SIZE) + 4 + 128;
   static char chunk [SIZE_TO_SEND];
   if (size != NULL) *size = SIZE_TO_SEND;
   
   chunk[0] = (uint32_t) list.size()*DISK_PAGE_SIZE; //!First 32b for the length
   memcpy (chunk[4], file_name, 128);

   int i = 132;
   for (list<void*>::iterator it = list_chunk.begin();
        it != list_chunk.end(); it++, i += DISK_PAGE_SIZE) {
     memcpy (chunk [DISK_PAGE_SIZE * i], *it, DISK_PAGE_SIZE);
   }

   return chunk;
  } 
 
  friend ostream& operator<< (ostream& os, Order o) {
   return o.serialize ();
  }

  //friend istream& operator>> (istream& is, Order o) {
  // return o.deserialize (is.);
  //}

}__attribute__((aligned));

#endif
