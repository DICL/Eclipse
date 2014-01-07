#include <UnitTest++.h>
#include <TestReporterStdout.h>
#include <omp.h>
#include <string.h>

using namespace UnitTest;

struct functor_test_A 
{
 bool operator () (const UnitTest::Test* _test) const {
  if (strcmp (_test->m_details.testName, "SERVER_MAIN"))
   return true;
  return false;
 }
};

struct functor_test_B
{
 bool operator () (const UnitTest::Test* _test) const {
  if (strcmp (_test->m_details.testName, "CLIENT_MAIN"))
   return true;

  return false;
 };
};

int main () {
  TestReporterStdout reporter;
  TestRunner runner (reporter);
 
#pragma omp parallel sections

#pragma omp section
 {
  runner.RunTestsIf (Test::GetTestList(), NULL, functor_test_A (), 0);
 }

 sleep (1); // Give sometime to get ready

#pragma omp section
 {
  runner.RunTestsIf (Test::GetTestList(), NULL, functor_test_B (), 0);
 }

}
