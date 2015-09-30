SUBDIRS             = . tests

AM_CPPFLAGS         = -I@srcdir@/src/common -I@srcdir@/src -include ./config.h
AM_CXXFLAGS         = -Wall -std=gnu++11 -ggdb3

bin_PROGRAMS        = master slave cache

# libs -----
lib_LTLIBRARIES     = libecfs.la
libecfs_la_SOURCES  = src/common/hash.cc src/common/settings.cc \
											src/common/fileclient.cc src/common/msgaggregator.cc src/common/util.cc src/common/mapreduce.cc \
											src/common/dataentry.cc src/common/datablock.cc src/common/messagebuffer.cc \
											src/common/logger.cc src/common/histogram.cc
libecfs_la_LDFLAGS  = -version-info 0:0:0

# Binaries ----
AM_LDFLAGS          = -static
LDADD               = libecfs.la

master_SOURCES      = src/master/master.cc 
slave_SOURCES       = src/slave/main.cc src/slave/slave.cc src/slave/slave_job.cc src/slave/slave_task.cc \
											src/cache_slave/cache_slave.cc src/cache_slave/filepeer.cc \
											src/cache_slave/writecount.cc src/cache_slave/iwriter.cc src/cache_slave/ireader.cc \
											src/cache_slave/filebridge.cc src/cache_slave/idistributor.cc \
											src/cache_slave/cache.cc src/cache_slave/file_connclient.cc

cache_SOURCES       = src/cache_new/application.cc src/cache_new/node.cc src/cache_new/master.cc \
                      src/cache_new/peer.cc \
                      src/cache_new/cache_slave.cc \
                      src/cache_new/main.cc

pkginclude_HEADERS  = src/common/ecfs.hh src/common/fileclient.hh \
                      src/common/msgaggregator.hh src/common/settings.hh src/common/dataentry.hh \
											src/common/messagebuffer.hh src/common/datablock.hh src/common/mapreduce.hh 
# Scripts -------
bin_SCRIPTS         = src/fs/eclipsed
# ------
if COPY_SAMPLES
sysconf_DATA        = doc/eclipse.json
endif

dist_doc_DATA       = README.md LICENSE doc/eclipse.json
man1_MANS           = doc/eclipsefs.1

EXTRA_DIST          = doc .autotools_aux $(shell find @srcdir@/src -name "*.hh" -o -name "*.in") \
										  doc/eclipse.json
