#AUTOMAKE_OPTIONS = subdir-objects #copy the structure of the source directory

AM_CPPFLAGS  = -I@top_srcdir@/src/common -I@top_srcdir@/src
AM_CPPFLAGS += -include ./config.h -include @top_srcdir@/lib/unittest-cpp/unittest++/UnitTest++.h
AM_CXXFLAGS  = -std=gnu++11 
AM_LDFLAGS   = -L @top_srcdir@/lib 
LDADD        = -lUnitTest++

TESTS = $(test_files)
check_PROGRAMS = $(test_files)
check_DATA = jsonfile
DISTCLEANFILES = eclipse.json

jsonfile:
	cp @top_srcdir@/src/sample/eclipse.json .

# Input your tests files here
test_files = settings

#Input the dependencies of the test files here
settings_SOURCES = settings_tests.cc ../src/common/settings.cc
