#ifndef __MAPREDUCE__
#define __MAPREDUCE__

#include <iostream>
#include <errno.h>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// user functions
void init_mapreduce (int argc, char** argv);   // initialize mapreduce configure
void summ_mapreduce(); // summarize mapreduce configure
void set_mapper (void (*map_func) (string));
void set_reducer (void (*red_func) (string key));
bool is_nextvalue(); // return true if there is next value
bool is_nextrecord(); // return true if there is next value
string get_nextvalue(); // returns values in reduce function
bool get_nextinput (string& inputpath);   // process to next input for map role
string get_nextrecord(); // return true when successful, false when out of input record
bool get_nextkey (string* value);   // return true when successful, false when out of key value pair
void add_inputpath (string path);
void set_outputpath (string path);
char** get_argv (void);   // get user argv excepting passed pipe fd
void write_keyvalue (string key, string value);
void write_output (string record);   // function used in reduce function
void enable_Icache(); // function that enables intermediate cache
void set_nummapper (int num);   // sets number of mappers
void set_numreducer (int num);   // sets number of reducers

int get_argc (void);   // get user argc excepting passed pipe fd
int connect_to_server (char *host, unsigned short port);
int get_jobid();


#endif
