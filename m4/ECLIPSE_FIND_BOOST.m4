dnl
dnl
AC_DEFUN([ECLIPSE_FIND_BOOST], 
[
# BOOST LIB {{{
have_boost=no
AC_CHECK_HEADERS([boost/foreach.hpp \
                  boost/property_tree/json_parser.hpp \
                  boost/property_tree/exceptions.hpp], [have_boost=yes])

if test "${have_boost}" = "no"; then
  echo -e "
-------------------------------------------------
 Unable to find Boost library in the canonical path
 Is it Centos? assuming path /usr/local/include/boost157/
-------------------------------------------------\n"

  #export CPLUS_INCLUDE_PATH='/usr/include/boost141'
  CPPFLAGS='-I /usr/local/include/boost157'
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
fi # }}}
])
