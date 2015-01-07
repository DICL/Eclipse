#include "../src/common/settings.hh"
#include <unittest++/UnitTest++.h>

struct Settings_fixture
{
  Settings* s ;
  Settings_fixture () { s = new Settings (); }
  ~Settings_fixture () { delete s; }
};


SUITE(SETTING_TESTS) {

  TEST_FIXTURE(Settings_fixture, one) {
    s->load_settings();
    CHECK_EQUAL (s->port(), 8000);
    CHECK_EQUAL (s->master_addr(), "192.168.1.201");

  }
}

int main () {
  return UnitTest::RunAllTests();
}
