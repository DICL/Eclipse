AM_CPPFLAGS      = -I@top_srcdir@/src/common -I@top_srcdir@/src
AM_CPPFLAGS     += -include ./config.h -include UnitTest++/UnitTest++.h
AM_CXXFLAGS      = -std=gnu++11
LDADD            = ../libecfs.la -lUnitTest++

check_PROGRAMS   = settings
check_SCRIPTS    = integration
check_DATA       = @top_srcdir@/doc/eclipse.json
CLEANFILES       = eclipse.json integration 

# Input your tests files here
TESTS            = settings integration 

# Input the dependencies of the test files here
settings_SOURCES = settings_tests.cc
