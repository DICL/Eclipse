#!/bin/sh

[ ! -e .autotools_aux ] && mkdir .autotools_aux

aclocal -I m4
autoconf
autoheader
automake --add-missing --copy
