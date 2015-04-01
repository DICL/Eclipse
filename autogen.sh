#!/bin/sh

[ ! -e .autotools_aux ] && mkdir .autotools_aux

aclocal -I m4                     # Generate aclocal
autoconf                          # Generate configure script 
autoheader                        # Generate config.h
automake --add-missing --copy     # Generate Makefile.in and other scripts 
