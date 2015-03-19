AC_INIT([Eclipse], [00.13.15])
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

dnl Example of default-enabled feature
#enable_samples=no
AC_ARG_ENABLE([samples],
    AS_HELP_STRING([--enable-samples], [copy sample of eclipse.json]))

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T
AC_HEADER_STDC


filesystem=DHT
AM_SILENT_RULES([yes])
AM_CONDITIONAL(FS_IS_DHT,[test "$filesystem" = "DHT"])
AM_CONDITIONAL(COPY_SAMPLES,[test "$enable_samples" = "yes"])

# Check for binaries
AC_CHECK_PROG([have_ssh], [ssh], [yes], [no])
AC_CHECK_PROG([have_scp], [scp], [yes], [no])
AC_CHECK_PROG([have_astyle], [astyle], [yes], [no])

AC_LANG([C++])
# BOOST LIB {{{
have_boost=no
AC_CHECK_HEADERS([boost/foreach.hpp \
                  boost/property_tree/json_parser.hpp \
                  boost/property_tree/exceptions.hpp], [have_boost=yes])

if test "${have_boost}" = "no"; then
  echo -e "
-------------------------------------------------
 Unable to find Boost library in the canonical path
 Is it Centos? assuming path /usr/include/boost141/
-------------------------------------------------\n"

  #export CPLUS_INCLUDE_PATH='/usr/include/boost141'
  CPPFLAGS='-I /usr/include/boost141'
  AS_UNSET([ac_cv_header_boost_foreach_hpp])
  AS_UNSET([ac_cv_header_boost_property_tree_json_parser_hpp])
  AS_UNSET([ac_cv_header_boost_property_tree_exceptions_hpp])

  AC_CHECK_HEADERS([boost/foreach.hpp \
                  boost/property_tree/json_parser.hpp \
                  boost/property_tree/exceptions.hpp], [have_boost=yes])

  if test "${have_boost}" = "no"; then
    WARN([
-------------------------------------------------
 I cannot find where you have the boost header files...
 Re-run configure script in this way:
 \033@<:@31m
   $ CPLUS_INCLUDE_PATH=/path/to/boost ./configure
 \033@<:@0m
-------------------------------------------------\n])
    AC_MSG_ERROR
  fi
fi 
# }}} 
# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h stddef.h stdint.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

AC_SEARCH_LIBS([pthread_create], [pthread])

FULLY_EVAL(["$sysconfdir"], [sysconfdirfull])

AH_TEMPLATE([ECLIPSE_PATH],[Root path for the Eclipse files])
AC_DEFINE_UNQUOTED([ECLIPSE_PATH], ["$prefix"])

AH_TEMPLATE([ECLIPSE_CONF_PATH],[Configuration files (ETC) for Eclipse])
AC_DEFINE_UNQUOTED([ECLIPSE_CONF_PATH], ["$sysconfdirfull"])

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
 Have Boost.......: $have_boost
 Have ssh?........: $have_ssh
 Have scp?........: $have_scp
 Have astyle?.....: $have_astyle
 Copy samples?....: $enable_samples
 
 Now type 'make @<:@-j<processors>@:>@ @<:@<target>@:>@'
 where processors indicate how many threads you want to use
 
 where the optional <target> is:
 all|'nothing'      - build all binaries
 install            - install everything at $prefix
    
-------------------------------------------------])
# vim: foldmethod=marker
