#include <settings.hh>
#include <iostream>
#include <sstream>

using namespace std;

struct Settings_fixture: public Settings
{
  Settings_fixture () : Settings ("./")  { }
};

SUITE(SETTING_TESTS) 
{
  TEST_FIXTURE(Settings_fixture, basic) 
  {
    load_settings();
    CHECK_EQUAL (port(), 8008);
    CHECK_EQUAL (master_addr(), "192.168.1.201");
    vector<string> test = nodelist();

    int i = 1;
    for (auto it = test.begin(); it != test.end(); it++, i++) 
    {
      ostringstream tmp;
      tmp << "192.168.1." << i; 
      string _ip = tmp.str();
      cout << *it << endl;
      CHECK_EQUAL (*it, _ip);
    }
  }
}

int main () {
  return UnitTest::RunAllTests();
}
