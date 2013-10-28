#ifndef _MAPREDUCE_
#define _MAPREDUCE_

#include <stdlib.h>
#include <sys/fcntl.h>
#include <iostream>

#define MR_PATH "/home/youngmoon01/MRR_storage/"
#define LIB_PATH "/home/youngmoon01/MRR/MRR/src_mr/"
#define BUF_SIZE 256

enum role
{
	MASTER,
	MAP,
	REDUCE
};

void init_mapreduce(int argc, char** argv); // initialize mapreduce configure
void summ_mapreduce(); // summarize mapreduce configure
int get_argc(void); // get user argc excepting passed pipe fd
char** get_argv(void); // get user argv excepting passed pipe fd
void set_mapper(void (*map_func) (void));
void set_reducer(void (*red_func) (void));
void set_splitter(void (*split_func) (void));
void word_splitter(void);
void byte_splitter(void);
void sentence_splitter(void);
void set_nummap(int num);
void set_numreduce(int numreduce);

static role status;
int pipefd[2]; // pipe fd to the program caller(master or slave) pipefd[0]: read, pipefd[1]: write
int argcount; // user argc
char** argvalues; // user argv
char mr_read_buf[BUF_SIZE]; // read buffer for pipe
char mr_write_buf[BUF_SIZE]; // write buffer for pipe

// variables for master status
int nummap = -1;
int numreduce = -1;
int completed_map = 0;
int completed_reduce = 0;
bool isset_mapper = false;
bool isset_reducer = false;
bool isset_splitter = false;
// variables for master status


void init_mapreduce(int argc, char** argv)
{
	int readbytes; // number of bytes read from pipe fd

	pipefd[0] = atoi(argv[argc-2]); // read pipe
	pipefd[1] = atoi(argv[argc-1]); // write pipe

	argcount = argc-2; // user argc
	argvalues = (char**)malloc(sizeof(char*)*argc-2);
	for(int i=0; i<argc-2; i++) // copy argv into argvalues to get user argv
	{
		argvalues[i] = (char*)malloc(sizeof(argv[i]));
		strcpy(argvalues[i], argv[i]);
	}

	write(pipefd[1], "whatisrole", BUF_SIZE);

	// blocking read from job caller
	while(1)
	{
		readbytes = read(pipefd[0], mr_read_buf, BUF_SIZE);
		if(readbytes == 0) // master abnormally terminated
		{
			// TODO: Terminate the job properly
			exit(0);
		}
		else if(readbytes < 0)
		{
			// sleeps for 0.01 seconds. change this if necessary
			usleep(10000);
			continue;
		}
		else // response received
		{
			if(strncmp(mr_read_buf, "master", 6) == 0)
			{
				status = MASTER;
				break;
			}
			else if(strncmp(mr_read_buf, "map", 3) == 0)
			{
				status = MAP;
				break;
			}
			else if(strncmp(mr_read_buf, "reduce", 6) == 0)
			{
				status = REDUCE;
				break;
			}
			else
			{
				// TODO: the message may not be response to the "whatisrole". deal with the case
			}
		}
		break;
		// sleeps for 0.01 seconds. change this if necessary
		usleep(10000);
	}
}

void summ_mapreduce()
{
	int readbytes;
	//TODO: make sure that all configuration are done
	
	if(status == MASTER) // running job
	{
		//TODO: manage all things if the status is master


		// clear up all things and complete successfully
		write(pipefd[1], "successfulcompletion", BUF_SIZE);
		// blocking read from master until "terminate" receiving message
		while(1)
		{
			readbytes = read(pipefd[0], mr_read_buf, BUF_SIZE);
			if(readbytes == 0) // master abnormally terminated
			{
				// TODO: Terminate the job properly
				exit(0);
			}
			else if(readbytes < 0)
			{
				// sleeps for 0.01 seconds. change this if necessary
				usleep(10000);
				continue;
			}
			else
			{
				if(strncmp(mr_read_buf, "terminate", 9) == 0) // "terminate" message received
					break;
				else // all other messages are ignored
					continue;
			}
		}

		close(pipefd[0]);
		close(pipefd[1]);
		exit(0);
	}
	else if(status == MAP) // map task
	{

	}
	else // reduce task
	{

	}
}

int get_argc(void)
{
	return argcount;
}

char** get_argv(void)
{
	return argvalues;
}
void set_mapper(void (*map_func) (void))
{
	
}

void set_reducer(void (*red_func) (void))
{
	
}

void set_splitter(void (*split_func) (void))
{
	
}

void word_splitter(void)
{
	
}

void byte_splitter(void)
{
	
}

void sentence_splitter(void)
{
	
}

void set_nummap(int num)
{
	if(num >= 0)
		nummap = num;
}

void set_numreduce(int num)
{
	if(num >= 0)
		numreduce = num;
}

#endif
