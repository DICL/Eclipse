AC_INIT([Eclipse], [00.18.04], [vicente.bolea@gmail.com], [], [dicl.unist.ac.kr])
AC_CONFIG_AUX_DIR([./.autotools_aux])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_SILENT_RULES([yes])
LT_INIT([dlopen])

AC_CONFIG_MACRO_DIRS([./m4/])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile tests/Makefile])
AC_CONFIG_FILES([src/fs/ecfs],        [chmod +x src/fs/ecfs])
AC_CONFIG_FILES([tests/integration],  [chmod +x tests/integration])

test -z "$CPPFLAGS" &&
  CXXFLAGS='-g -O0 -march=native'

AC_PROG_CXX
AC_LANG([C++])

# Argumeznts {{{
#enable_samples=no
AC_ARG_ENABLE([samples],
    AS_HELP_STRING([--disable-samples], [copy sample of eclipse.json]))

default_fs="DHT"
AC_ARG_WITH([fs],
    [AS_HELP_STRING([--with-fs], [File system <DHT|HDFS> ])],
    [fs="$withval"], [fs="$default_fs"]) 

# Configure options: --enable-debug[=no].
AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug], [enable debug code (default is no)])],
    [debug="$withval"], [debug=no])

AM_CONDITIONAL(FS_IS_DHT,[test $fs = DHT])
AM_CONDITIONAL(COPY_SAMPLES,[test "$enable_samples" != no ])

#}}}
# Dependencies checkings {{{

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h sys/socket.h sys/time.h unistd.h])
AC_SEARCH_LIBS([pthread_create], [pthread])

# Check for binaries
AC_CHECK_PROG([have_ssh], [ssh], [yes], [no])
AC_CHECK_PROG([have_scp], [scp], [yes], [no])
AC_CHECK_PROG([have_astyle], [astyle], [yes], [no])

have_ruby=no
have_unittest=no
ECLIPSE_FIND_BOOST
AX_PROG_RUBY_VERSION([2.1.0], [have_ruby=yes], [have_ruby=no])
#AC_CHECK_HEADERS([UnitTest++/UnitTest++.h], [have_unittest=yes], [have_unittest=no])
#AC_CHECK_LIB([UnitTest++], [UnitTest::RunAllTests])
PKG_CHECK_EXISTS([UnitTest++], [have_unittest=yes], [have_unittest=no])


#}}}
# PATHS {{{
FULLY_EVAL(["$sysconfdir"], [sysconfdirfull])
AC_SUBST(sysconfdirfull)

FULLY_EVAL(["$bindir"], [bindirfull])
AC_SUBST(bindirfull)

FULLY_EVAL(["$libdir"], [libdirfull])
AC_SUBST(libdirfull)

AH_TEMPLATE([ECLIPSE_PATH],[Root path for the Eclipse files])
AC_DEFINE_UNQUOTED([ECLIPSE_PATH], ["$prefix"])

AH_TEMPLATE([ECLIPSE_CONF_PATH],[Configuration files (ETC) for Eclipse])
AC_DEFINE_UNQUOTED([ECLIPSE_CONF_PATH], ["$sysconfdirfull"])
#}}}
#OUTPUT{{{
AC_OUTPUT
WARN([\
-------------------------------------------------
 \033@<:@31m
 $PACKAGE_NAME Version $PACKAGE_VERSION
 \033@<:@0m
 Paths & Settings
 -----
 Prefix...........: $prefix
 Sysconfdir(etc)..: $sysconfdirfull
 C++ Compiler.....: $CXX $CXXFLAGS $CPPFLAGS
 LIBS.............: $LIBS
 Copy samples?....: ${enable_samples:-NO}
 File System?.....: $fs

 Dependencies
 ------------
 [Required]
 Boost path.......: ${BOOST:-DEFAULT}
 Have Boost?......: $have_boost
 Have Ruby?.......: $have_ruby
 Have ssh?........: $have_ssh
 Have scp?........: $have_scp

 [Optional]
 Have UnitTest++?.: $have_unittest
 Have astyle?.....: $have_astyle

 
 Now type 'make @<:@-j<processors>@:>@ @<:@<target>@:>@'
 where processors indicate how many threads you want to use
 
 where the optional <target> is:
 all|'nothing'      - build all binaries
 install            - install everything at $prefix
    
-------------------------------------------------]) #}}}
# vim: foldmethod=marker
