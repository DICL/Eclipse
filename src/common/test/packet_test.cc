#include <packet.hh>
#include <UnitTest++.h>
#include <string.h>
//
// -----------------------------------------------------
//
struct fix_Packet {
 Packet* victim;

 fix_Packet () {
  victim = new Packet (new File ("file.txt"));
 }
 ~fix_Packet () {
  delete victim;
 }
};
//
// -----------------------------------------------------
//
SUITE (Packet) {
 // ----------------------------------------------------
 TEST_FIXTURE (fix_Packet, ctor_dtor) { }

 // ----------------------------------------------------
 TEST_FIXTURE (fix_Packet, File_cast) {
  File* file = static_cast<File*> (*victim);
  CHECK_EQUAL(file->get_filename(), "file.txt");
 }

 // ----------------------------------------------------
 TEST_FIXTURE (fix_Packet, Metadata_cast) {
  Metadata* first = new Metadata ();
  first->set_index (100); 

  victim->set_metadata (*first);
  Metadata* meta = static_cast<Metadata*> (*victim);

  CHECK_EQUAL(meta->get_index (), first->get_index());
 }
}
// -----------------------------------------------------
