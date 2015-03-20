AUTOMAKE_OPTIONS = subdir-objects #copy the structure of the source directory

AM_CPPFLAGS = -I@srcdir@/src/common -I@srcdir@/src -include ./config.h
AM_CXXFLAGS = -Wall -std=gnu++98 

bin_PROGRAMS = client slave master eclipse mcc cacheserver mrcat_core mrrm_core fd_core

if FS_IS_DHT
client_SOURCES = src/client/dht/client.cc src/common/settings.cc
slave_SOURCES =  src/slave/dht/slave.cc src/common/settings.cc
master_SOURCES = src/master/dht/master.cc src/common/settings.cc src/common/hash.cc
mcc_SOURCES = src/mcc/dht/mcc.cc src/common/settings.cc
endif

eclipse_SOURCES = src/orthrus/launcher.cc src/common/settings.cc src/common/hash.cc 
eclipse_CXXFLAGS = $(AM_CXXFLAGS) -Wno-maybe-uninitialized
cacheserver_SOURCES = src/orthrus/cacheserver.cc src/common/settings.cc
mrcat_core_SOURCES = src/file_distributor/mrcat_core.cc src/common/settings.cc src/common/hash.cc
mrrm_core_SOURCES = src/file_distributor/mrrm_core.cc src/common/settings.cc src/common/hash.cc
fd_core_SOURCES = src/file_distributor/fd_core.cc src/common/settings.cc src/common/hash.cc

bin_SCRIPTS  = $(wildcard @srcdir@/src/bin/*)

if COPY_SAMPLES
sysconf_DATA = src/sample/eclipse.json
endif

EXTRA_DIST = lib doc tests .autotools_aux src
doc_DATA = README.md LICENSE

distclean-local:
	-rm -rf @srcdir@/{autom4te.cache,aclocal.m4,config.h.in,*~,configure,Makefile.in,.autotools_aux}

clean-local:
	-rm src -rf 

install-data-hook: ; (\
  input_path="@prefix@"                                           ;\
  path=$${input_path//\//\\/}                                     ;\
  input_confpath="@sysconfdir@"                                   ;\
  confpath=$${input_confpath//\//\\/}                             ;\
  for i in @bindir@/*; do                                          \
    if [[ ! `file $${i}` =~ "ELF" ]]; then                         \
      sed -i "s/\$$ECLIPSE_PATH/$${path}/g" $$i                   ;\
      sed -i "s/\$$ECLIPSE_CONF_PATH/$${confpath}/g" $$i          ;\
    fi                                                             \
  done)
