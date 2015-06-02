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

For single user installation for developers
-------------------------------------------

    $ mkdir -p local_eclipse/{tmp,sandbox}                 # Create a sandbox directories
    $ cd local_eclipse                                     # enter in the directory
    $ git clone *eclipse git URL*                          # Clone the project from github
    $ sh autogen.sh                                        #
    $ cd ../tmp                                            # 
    $ sh ../Eclipse/configure --prefix=`pwd`/../sandbox

    ### This last command will be needed whenever you want to recompile the source
    $ make [-j#] install                                   # Compile & install add -j flag to speed up

Now edit in your **~/.bashrc** or **~/.profile**:

    export PATH="/home/*..PATH/To/eclipse/..*/sandbox/bin"
    export LIBRARY_PATH="/home/*..PATH/To/eclipse/..*/sandbox/lib"
    export C_INCLUDE_PATH="/home/*..PATH/To/eclipse/..*/sandbox/include"
    export MANPATH=`manpath`:/home*..PATH/To/eclipse/..*/sandbox/share/man

For the configuration refer to the manpage:

    $ man eclipsefs

AUTHOR
======
 - __AUTHOR:__ [Young Moon Eun] [ym]
 - __AUTHOR:__ [Vicente Adolfo Bolea Sanchez] [vicente]
 - __INSTITUTION:__ [DICL laboratory] [dicl] at [UNIST]

<!-- Links -->
[vicente]:  https://github.com/vicentebolea
[ym]:       https://github.com/youngmoon01
[dicl]:     http://dicl.unist.ac.kr
