AM_CPPFLAGS  = -I@top_srcdir@/src/common -I@top_srcdir@/src
AM_CPPFLAGS += -include ./config.h -include unittest++/UnitTest++.h
AM_CXXFLAGS  = -std=gnu++11
LDADD        = -lUnitTest++

TESTS = $(test_files)
check_PROGRAMS = $(test_files)
check_DATA = jsonfile
DISTCLEANFILES = eclipse.json

jsonfile: ; cp @top_srcdir@/doc/eclipse.json .

# Input your tests files here
test_files = settings

# Input the dependencies of the test files here
settings_SOURCES = settings_tests.cc ../src/common/settings.cc
