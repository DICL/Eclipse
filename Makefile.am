AM_CPPFLAGS  = -I@top_srcdir@/src/common -I@top_srcdir@/src
AM_CPPFLAGS += -include ./config.h -include @top_srcdir@/lib/unittest-cpp/unittest++/UnitTest++.h
AM_CXXFLAGS  = -std=gnu++11 
AM_LDFLAGS   = -L @top_srcdir@/lib 
LDADD        = ./libsettings.a -lUnitTest++ 

noinst_LIBRARIES = libsettings.a
libsettings_a_SOURCES = ../src/common/settings.cc ../src/common/settings.hh

TESTS = $(test_files)
check_PROGRAMS = $(test_files)
check_DATA = jsonfile

jsonfile:
	cp @top_srcdir@/src/sample/eclipse.json .

# Input your tests files here
test_files = settings_tests

#Input the dependencies of the test files here
#settings_SOURCES = settings_tests.cc
AM_DEFAULT_SOURCE_EXT = .cc
