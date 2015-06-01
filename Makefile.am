SUBDIRS             = . tests

AM_CPPFLAGS         = -I@srcdir@/src/common -I@srcdir@/src -include ./config.h
AM_CXXFLAGS         = -Wall -std=gnu++98

bin_PROGRAMS        = client slave master eclipse cacheserver

# libs -----
lib_LTLIBRARIES     = libecfs.la
libecfs_la_SOURCES  = src/common/hash.cc src/common/settings.cc \
											src/common/fileclient.cc src/common/msgaggregator.cc src/common/util.cc src/common/mapreduce.cc \
											src/common/dataentry.cc src/common/datablock.cc src/common/messagebuffer.cc
libecfs_la_CXXFLAGS = $(AM_CXXFLAGS) # -shared
libecfs_la_LDFLAGS  = -version-info 0:0:0

if FS_IS_DHT
client_SOURCES      = src/client/client.cc
client_LDADD        = libecfs.la
slave_SOURCES       = src/slave/slave.cc 
slave_LDADD         = libecfs.la
master_SOURCES      = src/master/master.cc 
master_LDADD        = libecfs.la
endif

eclipse_SOURCES     = src/file_distributor/launcher.cc
eclipse_CXXFLAGS    = $(AM_CXXFLAGS) -Wno-maybe-uninitialized
eclipse_LDADD       = libecfs.la
cacheserver_SOURCES = src/cacheserver/cacheserver.cc 
cacheserver_LDADD   = libecfs.la

pkginclude_HEADERS  = src/common/ecfs.hh src/common/file_connclient.hh src/common/fileclient.hh \
                      src/common/msgaggregator.hh src/common/settings.hh src/common/dataentry.hh \
											src/common/messagebuffer.hh src/common/datablock.hh src/common/mapreduce.hh
# Scripts -------
bin_SCRIPTS         = src/fs/ecfs
# ------
if COPY_SAMPLES
sysconf_DATA        = doc/eclipse.json
endif

dist_doc_DATA       = README.md LICENSE doc/eclipse.json

EXTRA_DIST          = lib doc .autotools_aux $(shell find @srcdir@/src -name "*.hh" -o -name "*.in") \
											$(sh_scripts) doc/eclipse.json
