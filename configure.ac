AC_INIT([Eclipse], [00.12.23])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_PROG_MAKE_SET

if test -z ${CXXFLAGS}; then
  CXXFLAGS='-g -O2 -march=native'
fi
AC_PROG_CXX

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

AC_HEADER_STDC

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

if test "x${have_boost}" = xno; then
  echo -e "
------------------------------------------------------
  Unable to find Boost library in the canonical path
  Is it Centos? assuming path /usr/include/boost141/
------------------------------------------------------\n"

  CPPFLAGS='-I /usr/include/boost141'
  AS_UNSET([ac_cv_header_boost_foreach_hpp])
  AS_UNSET([ac_cv_header_boost_property_tree_json_parser_hpp])
  AS_UNSET([ac_cv_header_boost_property_tree_exceptions_hpp])

  AC_CHECK_HEADERS([boost/foreach.hpp \
                  boost/property_tree/json_parser.hpp \
                  boost/property_tree/exceptions.hpp], [have_boost=yes])

  if test "x${have_boost}" = xno; then
    echo -e "
------------------------------------------------------
  I cannot find where you have the boost header files.
  Re-run configure script in this way:
  \033@<:@31m
    $ CPLUS_INCLUDE_PATH=/path/to/boost ./configure
  \033@<:@0m
------------------------------------------------------\n"
    AC_MSG_ERROR
  fi
fi 
# }}} 
# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h stddef.h stdint.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

AC_SEARCH_LIBS([pthread_create], [pthread])

AC_OUTPUT
echo -e "\
-------------------------------------------------
 \033@<:@31m
 ${PACKAGE_NAME} Version ${PACKAGE_VERSION}
 \033@<:@0m
 Prefix.......: $prefix
 C++ Compiler.: $CXX $CXXFLAGS $CPPFLAGS
 LIBS.........: $LIBS
 DEFS.........: $DEFS
 Have ssh?....: $have_ssh
 Have scp?....: $have_scp
 Have astyle?.: $have_astyle
 
 Now type 'make FS=@<:@<type>@:>@ @<:@<target>@:>@'
 where type is:
 dht                - description
 nfs                - description
 hdfs               - description
 
 where the optional <target> is:
 all|'nothing'      - build all binaries
 install            - install everything
    
--------------------------------------------------"
# vim: foldmethod=marker
