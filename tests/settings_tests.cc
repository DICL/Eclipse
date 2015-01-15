#include "../src/common/settings.hh"
#include <unittest++/UnitTest++.h>
#include <iostream>
#include <sstream>

using namespace std;

struct Settings_fixture
{
  Settings* s ;
  Settings_fixture () { s = new Settings (); }
  ~Settings_fixture () { delete s; }
};

SUITE(SETTING_TESTS) 
{
  TEST_FIXTURE(Settings_fixture, basic) 
  {
    s->load_settings();
    CHECK_EQUAL (s->port(), 8000);
    CHECK_EQUAL (s->master_addr(), "192.168.1.201");
    vector<string> test = s->nodelist();

    int i = 1;
    for (vector<string>::iterator it = test.begin(); it != test.end(); it++, i++) 
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
