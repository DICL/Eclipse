#include <order.hh>
#include <string.h>
#include <math.h>

Order::Order (char* file_name, uint8_t* chunk, size_t size) {
 memcpy (this->file_name, file_name, 128);

 for (size_t i = 0; i < ceil((double) size / DISK_PAGE_SIZE); i++) {
  uint8_t* chunky = new uint8_t [DISK_PAGE_SIZE];
  memcpy (chunky, (const void*) (chunk + i * DISK_PAGE_SIZE), DISK_PAGE_SIZE);
  list_chunk.push_back (chunky);
 }
}

Order& Order::set_data (uint8_t* data) {
 size_t size = sizeof (data);
 size_t i = size, j = 0;

 while (i > 0) {
   uint8_t * tmp = new uint8_t [DISK_PAGE_SIZE];

   if (size < DISK_PAGE_SIZE) {
    memcpy (tmp, (data + (j++ * DISK_PAGE_SIZE)), size); //SIGSEGV
    i -= size; 

   } else {
    memcpy (tmp, (data + (j++ * DISK_PAGE_SIZE)), DISK_PAGE_SIZE); //SIGSEGV
    i -= DISK_PAGE_SIZE;
   }

   list_chunk.push_back (tmp);
 }
 return *this; 
}

void Order::deserialize (uint8_t* chunk) {
 uint32_t size;
 memcpy (&size, (chunk + 0), 4);
 memcpy (file_name, (chunk + 4), 128);

 for (size_t i = 0; i < size; i++) {
  uint8_t* chunky = new uint8_t [DISK_PAGE_SIZE];
  if ( size < DISK_PAGE_SIZE) {
    memcpy (chunky, (chunk + 132 + (i * DISK_PAGE_SIZE)), DISK_PAGE_SIZE); //SIGSEGV
    i += size - 1; 
  } else {
    memcpy (chunky, (chunk + 132 + (i * DISK_PAGE_SIZE)), DISK_PAGE_SIZE); //SIGSEGV
    i += DISK_PAGE_SIZE - 1; 
  }
  list_chunk.push_back (chunky);
 }  
}

uint8_t* Order::serialize (size_t* size = NULL) {
 if (list_chunk.empty()) return NULL;
 size_t SIZE_TO_SEND = (size_t) (list_chunk.size () * DISK_PAGE_SIZE) + 4 + 128;
 uint8_t *chunk = new uint8_t [SIZE_TO_SEND];
 if (size != NULL) *size = SIZE_TO_SEND;

 uint32_t s = list_chunk.size() * DISK_PAGE_SIZE; //!First 32b for the length
 memcpy ((chunk + 0), &s, 4);
 memcpy ((chunk + 4), file_name, 128);

 int index = 132, i = 0;
 for (list<uint8_t*>::iterator it = list_chunk.begin();
   it != list_chunk.end(); it++, i++) {
  memcpy ((chunk + index + (DISK_PAGE_SIZE * i)), *it, DISK_PAGE_SIZE); //SIGSEGV
 }

 return chunk;
}
