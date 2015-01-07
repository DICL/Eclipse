AC_INIT([Eclipse], [00.12.23])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADERS([config.h])

CXXFLAGS="$CXXFLAGS -std=gnu++98"
AC_PROG_CXX
AC_PROG_CC
AC_PROG_MAKE_SET

# Checks for libraries.
# FIXME: Replace `main' with a function in `-latex':
#AC_CHECK_LIB([atex], [main])
AC_CHECK_LIB([boost], [main])
# FIXME: Replace `main' with a function in `-lpthread':
AC_CHECK_LIB([pthread], [main])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h stddef.h stdint.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

#Parameters
AC_ARG_ENABLE([eclipse-path],
              AC_HELP_STRING([--eclipse-path=@<:@path@:>@], [Set the root of Eclipse]),
              libpath="${enableval}",
              libpath="/home/vicente/Eclipse/build/",
              )

AC_DEFINE_UNQUOTED(LIB_PATH, ["$libpath"], ["hey"])
AC_HEADER_STDC

AC_OUTPUT
echo \
"-------------------------------------------------

 ${PACKAGE_NAME} Version ${PACKAGE_VERSION}

 Prefix:   '${prefix}'.
 Compiler: '${CXX} ${CFLAGS} ${CPPFLAGS}'
 LIB_PATH: '$libpath'

 Now type 'make FS=@<:@<type>@:>@ @<:@<target>@:>@'
   where type is:
     dht                - description
     nfs                - description
     hdfs               - description

   where the optional <target> is:
     all|'nothing'      - build all binaries
     install            - install everything

--------------------------------------------------"
