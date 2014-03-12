#include <metadata.hh>
#include <UnitTest++.h>
//
// -----------------------------------------------------
//
struct fix_metadata {

 Metadata* _m;
 fix_metadata () { _m = new Metadata (); }
 ~fix_metadata () { delete _m; }

};
//
// -----------------------------------------------------
//
SUITE (METADATA) {
 // ----------------------------------------------------
 TEST_FIXTURE (fix_metadata, ctor_dtor) { }
}

