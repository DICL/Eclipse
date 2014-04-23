#include "TestReporterStdout.h"
#include <cstdio>

#include "TestDetails.h"

namespace UnitTest {

void TestReporterStdout::ReportFailure(TestDetails const& details, char const* failure)
{
#if defined(__APPLE__) || defined(__GNUG__)
    char const* const errorFormat = "\e[31mFAILURE\e[0m %15s @ %-15s because \'\e[1m%s\e[0m\' at line %i\n";
#else                                                                                 
    char const* const errorFormat = "\e[31mFAILURE\e[0m %15s @ %-15s because \'\e[1m%s\e[0m\' at line %i\n";
#endif

	using namespace std;
    printf(errorFormat,  details.testName,  details.filename, failure, details.lineNumber);
}

void TestReporterStdout::ReportTestStart (TestDetails const& test)
{
}

void TestReporterStdout::ReportTestFinish (TestDetails const& test, float secondsElapsed)
{
  printf ("\e[32mSUCCESS\e[0m %15s @ %-15s within \e[34m%10f s\e[0m\n", test.testName, test.filename, secondsElapsed);
}

void TestReporterStdout::ReportSummary(int const totalTestCount, int const failedTestCount,
                                       int const failureCount, float secondsElapsed)
{
	using namespace std;
    printf ("- - -OVERALL- - -\n");

    if (failureCount > 0)
        printf("\e[31mFAILURE\e[0m %d out of %d tests failed (%d failures) within \e[34m%.2f s\e[0m.\n",
               failedTestCount, totalTestCount, failureCount, secondsElapsed);
    else
        printf("\e[32mSUCCESS\e[0m (\e[34m%d\e[0m) tests \e[4m\e[34mPASSED\e[0m within \e[34m%.2f s\e[0m.\n", 
               totalTestCount, secondsElapsed);
}

}
