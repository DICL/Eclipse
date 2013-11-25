#include <task.hh>
#include <UnitTest++.h>
//
// --------UNIT TEST AUTOMATICALLY GENERATED------------
//
struct fix_Task {
 Task* victim;

 fix_Task () {
  victim = new Task ("/bin/date");
 }
 ~fix_Task () {
  delete victim;
 }
};
//
// --------UNIT TEST AUTOMATICALLY GENERATED------------
//
SUITE (TASK_TEST) {

 // ----------------------------------------------------
 TEST_FIXTURE (fix_Task, ctor_dtor) { }

 // ----------------------------------------------------
 TEST_FIXTURE (fix_Task, get_path) { 
  char path [128];
  victim->get_path (path);
  CHECK_EQUAL (const_cast<const char*> (path), "/bin/date");
 } 
}
// -----------------------------------------------------
