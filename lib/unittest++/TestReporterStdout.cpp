#include "TestReporterStdout.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "TestDetails.h"

namespace UnitTest {

 void TestReporterStdout::ReportFailure(TestDetails const& details, char const* failure)
 {
  char* errorFormat;
  if (isatty (fileno(stdout))) {
   errorFormat = strdup ("\e[37m+ \e[31mFAILURE\e[0m %25s @ %-25s because \'\e[1m%s\e[0m\' at line %i\n");
  } else {
   errorFormat = strdup ("+ FAILURE %25s @ %-25s because \'%s\' at line %i\n");
  }

  using namespace std;
  printf(errorFormat,  details.testName,  details.filename, failure, details.lineNumber);
 }

 void TestReporterStdout::ReportTestStart (TestDetails const& test)
 {
 }

 void TestReporterStdout::ReportTestFinish (TestDetails const& test, float secondsElapsed)
 {
  static bool first = false;
  if (!first) {
   printf ("- - -RUNNING TESTS- - -\n");
   first=true;
  }
  if (isatty (fileno(stdout))) {
   printf ("\e[37m+ \e[32mSUCCESS\e[0m %25s @ %-25s within \e[34m%10f s\e[0m\n", test.testName, test.filename, secondsElapsed);
  } else {
   printf ("+ SUCCESS %25s @ %-25s within %10f s\n", test.testName, test.filename, secondsElapsed);
  }
 }

 void TestReporterStdout::ReportSummary(int const totalTestCount, int const failedTestCount,
   int const failureCount, float secondsElapsed)
 {
  using namespace std;
  printf ("- - -OVERALL- - -\n");

  if (isatty (fileno(stdout))) {
   if (failureCount > 0)
    printf("\e[31mFAILURE\e[0m %d out of %d tests failed (%d failures) within \e[34m%.2f s\e[0m.\n",
      failedTestCount, totalTestCount, failureCount, secondsElapsed);
   else
    printf("\e[32mSUCCESS\e[0m (\e[34m%d\e[0m) tests \e[4m\e[34mPASSED\e[0m within \e[34m%.2f s\e[0m.\n", 
      totalTestCount, secondsElapsed);
  } else {
   if (failureCount > 0)
    printf("FAILURE %d out of %d tests failed (%d failures) within %.2f s.\n",
      failedTestCount, totalTestCount, failureCount, secondsElapsed);
   else
    printf("SUCCESS (%d) tests PASSED within %.2f s.\n", 
      totalTestCount, secondsElapsed);
  }
 }

}
