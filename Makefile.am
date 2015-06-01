SUBDIRS             = . tests

AM_CPPFLAGS         = -I@srcdir@/src/common -I@srcdir@/src -include ./config.h
AM_CXXFLAGS         = -Wall -std=gnu++98

bin_PROGRAMS        = client slave master eclipse cacheserver

# libs -----
lib_LTLIBRARIES     = libecfs.la
libecfs_la_SOURCES  = src/common/hash.cc src/common/settings.cc src/common/file_connclient.cc \
											src/common/fileclient.cc src/common/msgaggregator.cc src/common/util.cc src/common/mapreduce.cc
libecfs_la_CXXFLAGS = $(AM_CXXFLAGS) -shared
libecfs_la_LDFLAGS  = -version-info 0:0:0

if FS_IS_DHT
client_SOURCES      = src/client/dht/client.cc src/common/settings.cc
client_LDADD        = libecfs.la
slave_SOURCES       = src/slave/dht/slave.cc src/common/settings.cc
master_SOURCES      = src/master/dht/master.cc src/common/settings.cc src/common/hash.cc
endif

eclipse_SOURCES     = src/orthrus/launcher.cc src/common/settings.cc src/common/hash.cc
eclipse_CXXFLAGS    = $(AM_CXXFLAGS) -Wno-maybe-uninitialized
eclipse_LDDADD      = libecfs.la
cacheserver_SOURCES = src/orthrus/cacheserver.cc src/common/settings.cc




pkginclude_HEADERS  = src/common/file_connclient.hh src/common/fileclient.hh \
                      src/common/hash.hh src/common/msgaggregator.hh src/common/settings.hh
# Scripts -------
bin_SCRIPTS         = src/fs/ecfs
# ------
#if COPY_SAMPLES
#sysconf_DATA       = src/sample/eclipse.json
#endif

dist_doc_DATA       = README.md LICENSE doc/eclipse.json

EXTRA_DIST          = lib doc .autotools_aux $(shell find @srcdir@/src -name "*.hh" -o -name "*.in") \
											$(sh_scripts) src/sample/eclipse.json
