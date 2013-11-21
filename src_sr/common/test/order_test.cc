#include <iostream>
#include <stdint.h>
#include <inttypes.h>
#include <stdexcept>
#include <order.hh>
#include <UnitTest++.h>

struct fix_order {
  Order* o;

  uint8_t* chunk1;
  fix_order () {
    size_t s = 10;
    uint8_t data [4096] = {'A','A','A','A','A','A','A','A','A','A'};
    o = new Order ("example.txt", data, s);
  }
  
  ~fix_order () {
    delete o;
  }
};

SUITE (normal) { 

	// --------------------------------------------------------
	TEST_FIXTURE (fix_order, constructor) {
   CHECK (o != NULL);
 }

	// --------------------------------------------------------
	TEST_FIXTURE (fix_order, get_file_name) {
   const char * tmp = o->get_file_name ();
   CHECK (strncmp ( tmp, "example.txt", 128) == 0);
 }

	// --------------------------------------------------------
	TEST_FIXTURE (fix_order, set_file_name) {
   o->set_file_name ("guapo");
   CHECK (strncmp ( o->get_file_name (), "guapo", 128) == 0);
 }

	// --------------------------------------------------------
	TEST_FIXTURE (fix_order, set_data) {
   uint8_t tmp [8192];
   bzero (tmp, 8192);
   o->set_data (tmp); 
 } 

	// --------------------------------------------------------
	TEST_FIXTURE (fix_order, serialize) {
   size_t size;
   char fn [128];

   o->set_file_name ("guapo");
   uint8_t* tmp = o->serialize (&size);
   memcpy (fn, tmp + 4, 128);
    
   CHECK (strncmp (fn, "guapo", 128) == 0);
 }

	// --------------------------------------------------------
	TEST_FIXTURE (fix_order, deserialize) {
   char* fn = "guapo";
   uint32_t s = 232;
  
   memcpy (chunk1 + 0,   &s,    4);
   memcpy (chunk1 + 4,   fn,  128);
   memset (chunk1 + 132, 222, 100);
  
   o->deserialize (chunk1);
   CHECK (strncmp (o->get_file_name(), "guapo", 128) == 0);
 }
}
/*
SUITE (heavy) { 

	// --------------------------------------------------------
	TEST_FIXTURE (fix_order, constructor) {
 }

	// --------------------------------------------------------
	TEST_FIXTURE (fix_order, destructor) {
 }
 
	// --------------------------------------------------------
	TEST_FIXTURE (fix_order, serialize) {
 }

	// --------------------------------------------------------
	TEST_FIXTURE (fix_order, deserialize) {
 }
}

SUITE (wrong) { 

	// --------------------------------------------------------
	TEST_FIXTURE (fix_order, constructor) {
 }

	// --------------------------------------------------------
	TEST_FIXTURE (fix_order, destructor) {
 }
 
	// --------------------------------------------------------
	TEST_FIXTURE (fix_order, serialize) {
 }

	// --------------------------------------------------------
	TEST_FIXTURE (fix_order, deserialize) {
 }
}
*/
