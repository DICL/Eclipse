#ifndef __MAPREDUCE__
#define __MAPREDUCE__

#include <string>

// user functions
void init_mapreduce (int argc, char** argv);   // initialize mapreduce configure
void summ_mapreduce(); // summarize mapreduce configure

void enable_Icache(); // function that enables intermediate cache
void set_mapper (void (*map_func) (string));
void set_reducer (void (*red_func) (string key));
void set_nummapper (int num);   // sets number of mappers
void set_numreducer (int num);   // sets number of reducers
void set_outputpath (string path);
void add_inputpath (string path);

bool is_nextvalue(); // return true if there is next value
bool is_nextrecord(); // return true if there is next value

string get_nextvalue(); // returns values in reduce function
string get_nextrecord(); // return true when successful, false when out of input record
bool get_nextkey (string* value);   // return true when successful, false when out of key value pair
int get_jobid();

void write_keyvalue (string key, string value);
void write_output (string record);   // function used in reduce function

#endif
