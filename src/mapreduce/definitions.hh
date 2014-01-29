#ifndef _DEFINITIONS_
#define _DEFINITIONS_

#define MR_PATH "/home/youngmoon01/mr_storage/"
#define HDMR_PATH "/user/youngmoon01/mr_storage/"
#define LIB_PATH "/home/youngmoon01/MRR/src/"
#define BUF_SIZE 4096
#define BUF_CUT 256

#define HDFS_PATH "/home/youngmoon01/hadoop-2.2.0/include/"
#define JAVA_LIB "/home/youngmoon01/jdk1.7.0_17/jre/lib/amd64/server/"
#define HDFS_LIB "/home/youngmoon01/hadoop-2.2.0/lib/native/"
#define HADOOP_FLAG "-lhdfs"
#define JAVA_FLAG "-ljvm"


enum mr_role
{
	JOB,
	MAP,
	REDUCE
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
	COMPLETED_STAGE // not used but reserved
};

int nbread(int fd, void* buf, int count)
{
	return 0;
}

#endif
