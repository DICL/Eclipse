AC_INIT([Eclipse], [00.12.23])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_PROG_MAKE_SET

if test -z ${CXXFLAGS}; then
  CXXFLAGS='-g -O2 -march=native'
fi
AC_PROG_CXX

# Checks for boost library {{{
have_boost=yes
AC_LANG_PUSH([C++])
AC_CHECK_HEADERS([boost/foreach.hpp \
                  boost/property_tree/json_parser.hpp \
                  boost/property_tree/exceptions.hpp], [], [have_boost=no]) 

if test "x${have_boost}" = xno; then
  echo "Is it Centos? lets find it with other path..."
  AC_SUBST(include, ["-I /usr/include/boost141/"])
  AC_CHECK_HEADERS([boost/foreach.hpp \
                    boost/property_tree/json_parser.hpp \
                    boost/property_tree/exceptions.hpp], [],
                   [AC_MSG_ERROR([Impossible to find boost library])])
fi 
AC_LANG_POP([C++])
# }}} 
# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h stddef.h stdint.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

AC_HEADER_STDC

AC_OUTPUT
echo \
"-------------------------------------------------

 ${PACKAGE_NAME} Version ${PACKAGE_VERSION}
 
 Prefix:   '${prefix}'.
 Compiler: '${CXX} ${CXXFLAGS}'
 Includes: '${include}' 
 
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
