#ifndef _DEFINITIONS_
#define _DEFINITIONS_

#define MR_PATH "/home/youngmoon01/mr_storage/"
#define LIB_PATH "/home/youngmoon01/MRR/src/"
#define HDFS_PATH "/home/youngmoon01/hadoop-1.2.0/src/c++/libhdfs/"
#define BUF_SIZE 512

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

#endif
