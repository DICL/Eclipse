#include <file.hh>
#include <UnitTest++.h>
//
// -----------------------------------------------------
//
struct fix_File {
 File* victim;

 fix_File () {
  victim = new File ("file.txt");
 }
 ~fix_File () {
  delete victim;
 }
};
//
// -----------------------------------------------------
//
SUITE (File) {
 // ----------------------------------------------------
 TEST_FIXTURE (fix_File, ctor_dtor) { }
}
// -----------------------------------------------------
