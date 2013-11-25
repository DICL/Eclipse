#ifndef _MAPREDUCE_
#define _MAPREDUCE_

#include <stdlib.h>
#include <sys/fcntl.h>
#include <iostream>
#include <mapreduce/definitions.hh>


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
void set_inputpath(char* inputpath);
void set_outputpath(char* outputpath);

static mr_role role;
int pipefd[2]; // pipe fd to the program caller(master or slave) pipefd[0]: read, pipefd[1]: write
int argcount; // user argc
char** argvalues; // user argv
char read_buf[BUF_SIZE]; // read buffer for pipe
char write_buf[BUF_SIZE]; // write buffer for pipe

// variables for master role
int** maplocation;
int** reducelocation;
int nummap = -1;
int numreduce = -1;
int completed_map = 0;
int completed_reduce = 0;
bool isset_mapper = false;
bool isset_reducer = false;
bool isset_splitter = false;
// variables for master role

// variables for task role
int jobindex;
// variables for task role

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
		readbytes = read(pipefd[0], read_buf, BUF_SIZE);
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
			if(strncmp(read_buf, "job", 3) == 0)
			{
				role = JOB;
				break;
			}
			else if(strncmp(read_buf, "map", 3) == 0)
			{
				role = MAP;
				break;
			}
			else if(strncmp(read_buf, "reduce", 6) == 0)
			{
				role = REDUCE;
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
	// TODO: make sure that all configuration are done
	
	if(role == JOB) // running job
	{
		// TODO: manage all things if the role is master

		// clear up all things and complete successfully
		write(pipefd[1], "succompletion", BUF_SIZE); // successful completion
		// blocking read from master until "terminate" receiving message
		while(1)
		{
			readbytes = read(pipefd[0], read_buf, BUF_SIZE);
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
				if(strncmp(read_buf, "terminate", 9) == 0) // "terminate" message received
					break;
				else // all other messages are ignored
					continue;
			}
		}
		close(pipefd[0]);
		close(pipefd[1]);
		exit(0);
	}
	else if(role == MAP) // map task
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

void set_nummap(int num) // when num is zero, flexible number of mapper is activated
{
	if(num > 0)
		nummap = num;
	else if(num == 0) // flexible number of mapper
		nummap = 0;
	else // no mapper task
		nummap = -1;
}

void set_numreduce(int num) // when num is zero, flexible number of reducer is activated
{
	if(num > 0)
		numreduce = num;
	else if(num == 0) // flexible number of reducer
		numreduce = 0;
	else // no reducer task
		numreduce = -1;
}

void set_inputpath(char* inputpath)
{

}

void set_outputpath(char* outputpath)
{

}

#endif
