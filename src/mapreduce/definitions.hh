#ifndef __DEFINITIONS__
#define __DEFINITIONS__

#include <iostream>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <config.h>
//#include </home/youngmoon01/MRR/config.h> // [wb]

using namespace std;

// HDFS
#define HDMR_PATH "/user/youngmoon01/mr_storage/"
#define HDFS_PATH "/home/youngmoon01/hadoop-2.2.0/include/"
#define JAVA_LIB "/home/youngmoon01/jdk1.7.0_17/jre/lib/amd64/server/"
#define HDFS_LIB "/home/youngmoon01/hadoop-2.2.0/lib/native/"
#define HADOOP_FLAG "-lhdfs"
#define JAVA_FLAG "-ljvm"


#define DHT_PATH "/scratch2/youngmoon01/mr_storage/"
#define MR_PATH "/home/youngmoon01/mr_storage/"
#define IPC_PATH "/scratch/youngmoon01/socketfile"
#define LIB_PATH "/home/youngmoon01/MRR/src/"

#define BUF_SIZE (8*1024) // determines maximum size of a record
//#define BUF_SIZE (128*1024) // determines maximum size of a record
// #define BUF_SIZE 4194304


#define BUF_THRESHOLD (7*1024) // the buffer flush threshold 
//#define BUF_THRESHOLD (127*1024) // the buffer flush threshold
// #define BUF_THRESHOLD 4194303
// #define BUF_THRESHOLD 8191

//#define BUF_CUT 512
#define BUF_CUT 512

// #define CACHESIZE (3*1024) // 4 GB of cache size
#define CACHESIZE 0 // 4 GB of cache size

// #define PREFETCHING
// #define INJECT_TASK_OVERHEAD
// #define MAPNOREUSE

#define BLOCKSIZE (512*1024) // 512 KB sized block <- should be multiple of BUF_SIZE
//#define BLOCKSIZE (256*1024*1024) // 512 KB sized block <- should be multiple of BUF_SIZE
// #define BLOCKSIZE 4194304

//#define IBLOCKSIZE (32*1024*1024) // size of intermediate flush threshold
#define IBLOCKSIZE (64*1024*1024) // size of intermediate flush threshold

#define FILE_CHUNK_SIZE (64*1024*1024) // size of intermediate flush threshold
//#define FILE_CHUNK_SIZE (64*1024*1024) // size of intermediate flush threshold

#define METADATA_PORT 8932

// EM-KDE
#define ALPHA 0.5
#define NUMBIN 100 // number of histogram bins in the EM-KDE scheduling
#define UPDATEINTERVAL 5000 // update interval in msec
#define KERNELWIDTH 2 // number of bins affected by count_query() function: 1 + 2*KERNELWIDTH (except the boundary bins)

#define MAP_SLOT 8 // [wb]
#define REDUCE_SLOT 8 // [wb]


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

// non-blocking write
int nbwrite (int fd, char* buf, char* contents)     // when the content should be specified
{
    int written_bytes;
    strcpy (buf, contents);
	int writing_bytes = BUF_CUT * (strlen(contents) / BUF_CUT + 1);
    memset (buf + strlen(contents), 0, writing_bytes - strlen(contents));
    
    while ( (written_bytes = write (fd, buf, BUF_CUT * (strlen (buf) / BUF_CUT + 1))) < 0)
    {
        if (errno == EAGAIN)
        {
            // do nothing as default
        }
        else if (errno == EBADF)
        {
            cout << "\twrite function failed due to EBADF, retrying..." << endl;
        }
        else if (errno == EFAULT)
        {
            cout << "\twrite function failed due to EFAULT, retrying..." << endl;
        }
        else if (errno == EFBIG)
        {
            cout << "\twrite function failed due to EFBIG, retrying..." << endl;
        }
        else if (errno == EINTR)
        {
            cout << "\twrite function failed due to EINTR, retrying..." << endl;
        }
        else if (errno == EINVAL)
        {
            cout << "\twrite function failed due to EINVAL, retrying..." << endl;
        }
        else if (errno == EIO)
        {
            cout << "\twrite function failed due to EIO, retrying..." << endl;
        }
        else if (errno == ENOSPC)
        {
            cout << "\twrite function failed due to ENOSPC, retrying..." << endl;
        }
        else if (errno == EPIPE)
        {
            cout << "\twrite function failed due to EPIPE, retrying..." << endl;
        }
        else
        {
            cout << "\twrite function failed due to unknown reason(debug needed)..." << endl;
            return -1;
        }
        
        // sleep 1 milli seconds to prevent busy waiting
        usleep (1000);
    }
    
    if (written_bytes != BUF_CUT * ( (int) strlen (buf) / BUF_CUT + 1))
    {
        int progress = written_bytes;
        int remain = BUF_CUT * (strlen (buf) / BUF_CUT + 1) - written_bytes;
        
        while (remain > 0)
        {
            written_bytes = write (fd, buf + progress, remain);
            
            if (written_bytes > 0)
            {
                progress += written_bytes;
                remain -= written_bytes;
            }
            
            // sleep 1 milli seconds to prevent busy waiting
            usleep (1000);
        }
    }
    
    return written_bytes;
}

// non-blocking write
int nbwrite (int fd, char* buf)     // when the content is already on the buffer
{
/*
if(strncmp(buf, "Iread", 5) == 0) { // [wb]
	cout << "[nbwrite] \"" << buf << "\"" << endl;
}
*/
    int written_bytes;
	int writing_bytes = BUF_CUT * (strlen(buf) / BUF_CUT + 1);
    
    while ( (written_bytes = write (fd, buf, BUF_CUT * (strlen (buf) / BUF_CUT + 1))) < 0)
    {
        if (errno == EAGAIN)
        {
            // do nothing as default
        }
        else if (errno == EBADF)
        {
            cout << "\twrite function failed due to EBADF, retrying..." << endl;
            cout << "\tcontents: " << buf << endl;
            sleep (5);
        }
        else if (errno == EFAULT)
        {
            cout << "\twrite function failed due to EFAULT, retrying..." << endl;
            sleep (5);
        }
        else if (errno == EFBIG)
        {
            cout << "\twrite function failed due to EFBIG, retrying..." << endl;
            sleep (5);
        }
        else if (errno == EINTR)
        {
            cout << "\twrite function failed due to EINTR, retrying..." << endl;
            sleep (5);
        }
        else if (errno == EINVAL)
        {
            cout << "\twrite function failed due to EINVAL, retrying..." << endl;
            sleep (5);
        }
        else if (errno == EIO)
        {
            cout << "\twrite function failed due to EIO, retrying..." << endl;
            sleep (5);
        }
        else if (errno == ENOSPC)
        {
            cout << "\twrite function failed due to ENOSPC, retrying..." << endl;
            sleep (5);
        }
        else if (errno == EPIPE)
        {
            cout << "\twrite function failed due to EPIPE, retrying..." << endl;
            sleep (5);
        }
        else
        {
            cout << "\twrite function failed due to unknown reason(debug needed)..." << endl;
            sleep (5);
            return -1;
        }
        
        // sleep 1 milli seconds to prevent busy waiting
        usleep (1000);
    }
    
    if (written_bytes != writing_bytes)
    {
        int progress = written_bytes;
        int remain = writing_bytes - written_bytes;
        
        while (remain > 0)
        {
            written_bytes = write (fd, buf + progress, remain);
            
            if (written_bytes > 0)
            {
                progress += written_bytes;
                remain -= written_bytes;
            }
            
            // sleep 1 milli seconds to prevent busy waiting
            usleep (1000);
        }
    }
    
    return written_bytes;
}

// non-blocking read
int nbread (int fd, char* buf)
{

    int total_readbytes = 0;
    int readbytes = 0;
    memset (buf, 0, BUF_SIZE);
	//buf[BUF_CUT - 1] = 0;
    readbytes = read (fd, buf, BUF_CUT);
/*
if(strncmp(buf, "Iread", 5) == 0) { // [wb] 
	cout << "[nbread] " << buf << endl;
}   
*/
    if (readbytes == 0)
    {
        return readbytes;
    }
    else if (readbytes < 0)
    {
        if (errno != EAGAIN)
        {
            if (errno == EBADF)
            {
                cout << "\t\033[0;31mread function failed due to EBADF error, debug needed\033[0m" << endl;
            }
            else if (errno == EFAULT)
            {
                cout << "\t\033[0;31mread function failed due to EFAULT error, debug needed\033[0m" << endl;
            }
            else if (errno == EINTR)
            {
                cout << "\t\033[0;31mread function failed due to EINTR error, debug needed\033[0m" << endl;
            }
            else if (errno == EINVAL)
            {
                cout << "\t\033[0;31mread function failed due to EINVAL error, debug needed\033[0m" << endl;
            }
            else if (errno == EIO)
            {
                cout << "\t\033[0;31mread function failed due to EIO error, debug needed\033[0m" << endl;
            }
            else if (errno == EISDIR)
            {
                cout << "\t\033[0;31mread function failed due to EISDIR error, debug needed\033[0m" << endl;
            }
            else
            {
                cout << "\t\033[0;31mread function failed due to unspecified error, debug needed\033[0m" << endl;
            }
            
            // sleep 1 second for easier debug
            sleep (1);
        }
        
        return readbytes;
    }
    else
    {
        total_readbytes += readbytes;
        
        if (buf[total_readbytes - 1] == 0 && total_readbytes % BUF_CUT == 0)
        {
            return total_readbytes;
        }
        else
        {
            while (1)
            {
				//buf[total_readbytes + BUF_CUT - (total_readbytes % BUF_CUT) - 1] = 0;
                readbytes = read (fd, buf + total_readbytes, BUF_CUT - (total_readbytes % BUF_CUT));
                
                if (readbytes == 0)
                {
                    cout << "\t\033[0;32mthe fd was closed during reading the buffer: debug the nbread() function.\033[0m" << endl;
                    cout << "\t\033[0;32m" << buf << "\033[0m" << endl;
                    cout << "\t\033[0;32m" << "total_readbytes: " << total_readbytes << "\033[0m" << endl;
                    cout << "\t\033[0;32m" << "last_character: " << buf[total_readbytes - 1] << "\033[0m" << endl;
                    return 0;
                }
                else if (readbytes < 0)
                {
                    // sleep 1 milli seconds to prevent busy waiting
                    usleep (1000);
                    continue;
                }
                else
                {
                    total_readbytes += readbytes;
                    
                    if (buf[total_readbytes - 1] != 0 || total_readbytes % BUF_CUT != 0)
                    {
                        //usleep(1000);
                        continue;
                    }
                    else
                    {
                        return total_readbytes;
                    }
                }
            }
        }
    }
    
    return total_readbytes;
}

#endif
