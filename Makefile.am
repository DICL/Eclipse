SUBDIRS             = . tests

AM_CPPFLAGS         = -I@srcdir@/src/common -I@srcdir@/src -include ./config.h
AM_CXXFLAGS         = -Wall -std=gnu++11 -ggdb3

bin_PROGRAMS        = client slave master eclipse #cacheserver

# libs -----
lib_LTLIBRARIES     = libecfs.la
libecfs_la_SOURCES  = src/common/hash.cc src/common/settings.cc \
											src/common/fileclient.cc src/common/msgaggregator.cc src/common/util.cc src/common/mapreduce.cc \
											src/common/dataentry.cc src/common/datablock.cc src/common/messagebuffer.cc \
 											src/common/logger.cc
libecfs_la_LDFLAGS  = -version-info 0:0:0

# Binaries ----
AM_LDFLAGS          = -static
LDADD               = libecfs.la

client_SOURCES      = src/client/client.cc
slave_SOURCES       = src/slave/slave.cc 
master_SOURCES      = src/master/master.cc 
eclipse_SOURCES     = src/cache_slave/cache_slave.cc 
cacheserver_SOURCES = src/cache_server/cacheserver.cc 

pkginclude_HEADERS  = src/common/ecfs.hh src/common/fileclient.hh \
                      src/common/msgaggregator.hh src/common/settings.hh src/common/dataentry.hh \
											src/common/messagebuffer.hh src/common/datablock.hh src/common/mapreduce.hh
# Scripts -------
bin_SCRIPTS         = src/fs/ecfs src/fs/eclipse-debug
# ------
if COPY_SAMPLES
sysconf_DATA        = doc/eclipse.json
endif

dist_doc_DATA       = README.md LICENSE doc/eclipse.json
man1_MANS           = doc/eclipsefs.1

EXTRA_DIST          = doc .autotools_aux $(shell find @srcdir@/src -name "*.hh" -o -name "*.in") \
											$(sh_scripts) doc/eclipse.json
