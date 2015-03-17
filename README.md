[![Build Status](https://magnum.travis-ci.com/DICL/Eclipse.svg?token=MaWCP2sHsbC2FaU6ztsx)](https://magnum.travis-ci.com/DICL/Eclipse)

BRIEFING
========

Eclipse is a novel experimental MapReduce framework integrated with distributed
semantic caches and Chord based Distributed Hash Table file system.

Eclipse was created to satisfy the need for better performance in the Hadoop framework.
Eclipse outperforms better by addressing some key facts such as dealing with the 
HDFS storage bottleneck, providing a better concurrency scheduling, imposing austere 
memory management and implementing an efficient suffle phase. 

COMPILING & INSTALLING
=====================

Generate files needed for building eclipse

    $ autoreconf -i

If you are developing this project I recommend you to 
build it in a different folder, if not objects and binaries will
mix up with the sources. If so here type those commands:

    $ mkdir build && cd build
    
Call the configure script(use the prefix argument for a local installation)

    $ sh ../eclipse/path/../configure 

Compile and install
    $ make
    $ make install


TODO LIST
=========
 - [x] Merge all the branch to start new workflow
 - [ ] Integrate with GNU/lustre fs
 - [ ] Change Network current interface (sockets) to MPI

AUTHOR
======
 - __AUTHOR:__ [Young Moon Eun] [ym]
 - __AUTHOR:__ [Vicente Adolfo Bolea Sanchez] [vicente]
 - __INSTITUTION:__ [DICL laboratory] [dicl] at [UNIST]

<!-- Links -->
[vicente]:  https://github.com/vicentebolea
[ym]:       https://github.com/youngmoon01
[dicl]:     http://dicl.unist.ac.kr
