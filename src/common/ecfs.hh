#ifndef __ECFS_HH__
#define __ECFS_HH__

#include <string>
#include <vector>
#include <stddef.h>
#include <stdint.h>
#include <iostream>
#include <set>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fstream>
#include <fcntl.h>

#include <boost/property_tree/ptree.hpp>

#define HADOOP_FLAG "-lhdfs"
#define JAVA_FLAG "-ljvm"

#define BUF_SIZE (8*1024) // determines maximum size of a record
#define BUF_THRESHOLD (7*1024) // the buffer flush threshold
#define BUF_CUT 512
#define CACHESIZE (1*1024*1024*1024) // 1 GB of cache size
#define BLOCKSIZE (512*1024) // 512 KB sized block <- should be multiple of BUF_SIZE

#define IBLOCKSIZE (64*1024*1024) // size of intermediate flush threshold

// EM-KDE
#define ALPHA 0.001
#define NUMBIN 100 // number of histogram bins in the EM-KDE scheduling
#define UPDATEINTERVAL 5000 // update interval in msec
#define KERNELWIDTH 2 // number of bins affected by count_query() function: 1 + 2*KERNELWIDTH (except the boundary bins)

#define MAP_SLOT 16
#define REDUCE_SLOT 16


#define HASHLENGTH 64

#define BACKLOG 16384

enum mr_role
{
    JOB,
    MAP,
    REDUCE
};

enum datatype
{
    RAW,
    INTERMEDIATE,
    OUTPUT
};

enum task_status
{
    WAITING,
    RUNNING,
    COMPLETED
};

enum job_stage
{
    INITIAL_STAGE,
    MAP_STAGE,
    REDUCE_STAGE,
    COMPLETED_STAGE // not used but reserved for future use
};

enum bridgetype   // bridge source and destination type
{
    PEER,
    DISK,
    CACHE,
    CLIENT,
    DISTRIBUTE // distribute the intermediate result of specific app + input pair
};

enum transfertype   // data transfer type. packet or stream
{
    PACKET,
    STERAM
};

enum file_role
{
    READ,
    WRITE,
    UNDEFINED
};

using std::string;
using std::vector;
using std::set;
using namespace boost::property_tree;

// Hash function--------------------------------------------
#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

extern "C" uint32_t h (const char *, size_t);
//----------------------------------------------------------
int nbwrite (int, char*, char* contents);
int nbwrite (int, char*);
int nbread (int, char*);


#include "dataentry.hh"
#include "messagebuffer.hh"
#include "settings.hh"
#include "mapreduce.hh"
#include "fileclient.hh"
#include "msgaggregator.hh"

#endif
