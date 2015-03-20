AC_INIT([Eclipse], [00.13.15], [vicente.bolea@gmail.com])
AC_CONFIG_AUX_DIR([./.autotools_aux])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([./m4/])

AC_PROG_CC
AC_PROG_MAKE_SET

if test -z ${CXXFLAGS}; then
  CXXFLAGS='-g -O2 -march=native'
fi

AC_PROG_CXX
AC_LANG([C++])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T
AC_HEADER_STDC

# Arguments {{{
#enable_samples=no
AC_ARG_ENABLE([samples],
    AS_HELP_STRING([--enable-samples], [copy sample of eclipse.json]))

default_fs="DHT"
AC_ARG_WITH([fs],
    [AS_HELP_STRING([--with-fs], [File system <DHT|HDFS> ])],
    [fs="$withval"], [fs="default_fs"]) 

# Configure options: --enable-debug[=no].
AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug], [enable debug code (default is no)])],
    [debug="$withval"], [debug=no])

filesystem=DHT
AM_SILENT_RULES([yes])
AM_CONDITIONAL(FS_IS_DHT,[test "$fs" = "DHT"])
AM_CONDITIONAL(COPY_SAMPLES,[test "$enable_samples" = "yes"])

#}}}
# Dependencies checkings {{{
# Check for binaries
AC_CHECK_PROG([have_ssh], [ssh], [yes], [no])
AC_CHECK_PROG([have_scp], [scp], [yes], [no])
AC_CHECK_PROG([have_astyle], [astyle], [yes], [no])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h stddef.h stdint.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

AC_SEARCH_LIBS([pthread_create], [pthread])

ECLIPSE_FIND_BOOST
#}}}
FULLY_EVAL(["$sysconfdir"], [sysconfdirfull])

AH_TEMPLATE([ECLIPSE_PATH],[Root path for the Eclipse files])
AC_DEFINE_UNQUOTED([ECLIPSE_PATH], ["$prefix"])

AH_TEMPLATE([ECLIPSE_CONF_PATH],[Configuration files (ETC) for Eclipse])
AC_DEFINE_UNQUOTED([ECLIPSE_CONF_PATH], ["$sysconfdirfull"])

#OUTPUT{{{
AC_OUTPUT
WARN([\
-------------------------------------------------
 \033@<:@31m
 ${PACKAGE_NAME} Version ${PACKAGE_VERSION}
 \033@<:@0m
 Paths
 -----
 Prefix...........: $prefix
 Sysconfdir(etc)..: $sysconfdirfull

 Dependencies
 ------------
 C++ Compiler.....: $CXX $CXXFLAGS $CPPFLAGS
 LIBS.............: $LIBS
 Boost path.......: ${BOOST:-DEFAULT}
 Have Boost.......: $have_boost
 Have ssh?........: $have_ssh
 Have scp?........: $have_scp
 Have astyle?.....: $have_astyle
 Copy samples?....: $enable_samples
 File System?.....: $fs
 
 Now type 'make @<:@-j<processors>@:>@ @<:@<target>@:>@'
 where processors indicate how many threads you want to use
 
 where the optional <target> is:
 all|'nothing'      - build all binaries
 install            - install everything at $prefix
    
-------------------------------------------------]) #}}}
# vim: foldmethod=marker
