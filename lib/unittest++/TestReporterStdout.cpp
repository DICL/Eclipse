#include "TestReporterStdout.h"
#include <cstdio>

#include "TestDetails.h"

namespace UnitTest {

void TestReporterStdout::ReportFailure(TestDetails const& details, char const* failure)
{
#if defined(__APPLE__) || defined(__GNUG__)
    char const* const errorFormat = "\e[31mERROR %s:\e[1m%s\e[0m:\e[32m%d\e[0m: Failure in (%s)\n";
#else                                           
    char const* const errorFormat = "\e[31mERROR %s:\e[1m%s\e[0m:\e[32m%d\e[0m: Failure in (%s)\n";
#endif

	using namespace std;
    printf(errorFormat, details.filename, details.testName, details.lineNumber, failure);
}

void TestReporterStdout::ReportTestStart(TestDetails const& /*test*/)
{
}

void TestReporterStdout::ReportTestFinish(TestDetails const& /*test*/, float)
{
}

void TestReporterStdout::ReportSummary(int const totalTestCount, int const failedTestCount,
                                       int const failureCount, float secondsElapsed)
{
	using namespace std;

    if (failureCount > 0)
        printf("[\e[31mFAILURE\e[0m] %d out of %d tests failed (%d failures).\n", failedTestCount, totalTestCount, failureCount);
    else
        printf("[\e[32mSUCCESS\e[0m] (\e[34m%d\e[0m) tests \e[4m\e[34mPASSED\e[0m.\n", totalTestCount);

    printf("\e[34m[Test time] >>> %.2f seconds.\e[0m\n", secondsElapsed);
}

}
