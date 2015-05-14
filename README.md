[![Build Status](https://magnum.travis-ci.com/DICL/Eclipse.svg?token=MaWCP2sHsbC2FaU6ztsx)](https://magnum.travis-ci.com/DICL/Eclipse)

BRIEFING
========

Eclipse is a novel experimental MapReduce framework integrated with distributed
semantic caches and Chord based Distributed Hash Table file system.

Eclipse was created to satisfy the need for better performance in the Hadoop framework.
Eclipse outperforms better by addressing some key facts such as dealing with the 
HDFS storage bottleneck, providing a better concurrency scheduling, imposing austere 
memory management and implementing an efficient suffle phase. 

INSTALLATION
============

This project is packaged using GNU autotools (autoconf, automake and libtool) so the compilation & installation
follows the standard way, e.g: 
  
    | #Next three lines are needed for a fresh compilation (when the system is broken or new install)
    | ----------------------------------------------------------
    | $ git clean -Xf                 # Removes all the ingnored files (see .gitignore)
    | $ autoreconf -i                 # Generate the configure script
    | $ sh configure                  # Configure script will set up the Makefile.in and generate Makefile 
    | ---------------------------------------------------------- 
    | # Next two commands are needed whenever you want to recompile the project
    | $ make clean                    # Safety cleanup, you will need to exec it if you modify a .h/.hh file
    | $ make -j8                      # Compile it
    \/

Requirement for developers
-----------
 - GCC > 4.4
 - GNU autotools relativelly new
 - Boost library > 1.50
 
AUTHOR
======
 - __AUTHOR:__ [Young Moon Eun] [ym]
 - __AUTHOR:__ [Vicente Adolfo Bolea Sanchez] [vicente]
 - __INSTITUTION:__ [DICL laboratory] [dicl] at [UNIST]

<!-- Links -->
[vicente]:  https://github.com/vicentebolea
[ym]:       https://github.com/youngmoon01
[dicl]:     http://dicl.unist.ac.kr
