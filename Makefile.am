SUBDIRS             = . tests

AM_CPPFLAGS         = -I@srcdir@/src/common -I@srcdir@/src -include ./config.h
AM_CXXFLAGS         = -Wall -std=gnu++98

bin_PROGRAMS        = client slave master eclipse mcc cacheserver mrcat_core mrrm_core fd_core

if FS_IS_DHT
client_SOURCES      = src/client/dht/client.cc src/common/settings.cc
slave_SOURCES       = src/slave/dht/slave.cc src/common/settings.cc
master_SOURCES      = src/master/dht/master.cc src/common/settings.cc src/common/hash.cc
mcc_SOURCES         = src/mcc/dht/mcc.cc src/common/settings.cc
endif

eclipse_SOURCES     = src/orthrus/launcher.cc src/common/settings.cc src/common/hash.cc
eclipse_CXXFLAGS    = $(AM_CXXFLAGS) -Wno-maybe-uninitialized
cacheserver_SOURCES = src/orthrus/cacheserver.cc src/common/settings.cc
mrcat_core_SOURCES  = src/file_distributor/mrcat_core.cc src/common/settings.cc src/common/hash.cc
mrrm_core_SOURCES   = src/file_distributor/mrrm_core.cc src/common/settings.cc src/common/hash.cc
fd_core_SOURCES     = src/file_distributor/fd_core.cc src/common/settings.cc src/common/hash.cc

# Scripts -------
sh_scripts          = src/bin/mrclear_core src/bin/runmaster.sh src/bin/runslave.sh src/bin/jsawk
bin_SCRIPTS         = $(wildcard src/bin/*) $(sh_scripts)
# ------
if COPY_SAMPLES
sysconf_DATA        = src/sample/eclipse.json
endif

dist_doc_DATA       = README.md LICENSE

EXTRA_DIST          = lib doc .autotools_aux $(shell find @srcdir@/src -name "*.hh" -o -name "*.in") \
											$(sh_scripts) src/sample/eclipse.json
