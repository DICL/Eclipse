#ifndef __IWRITER__
#define __IWRITER__

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <mapreduce/definitions.hh>

using namespace std;

class iwriter
{
	private:
    char *write_buf;
		//bool iswriting;
		bool is_writing_array[REDUCE_SLOT];
		int jobid;
		int networkidx;
		//int numblock;
		int num_block[REDUCE_SLOT];
		//int filefd;
		int file_fd[REDUCE_SLOT];
		//int vectorindex;
		int vector_index[REDUCE_SLOT];

		//vector<map<string, vector<string>*>*> themaps;
		vector<map<string, vector<string>*>*> keyvalues_maps[REDUCE_SLOT];

		//int writingblock;
		int writing_block[REDUCE_SLOT];
		//int availableblock;
		int available_block[REDUCE_SLOT];
		//int pos; // position in the write_buf
		int position[REDUCE_SLOT];
		//map<string, vector<string>*>::iterator writing_it;
		map<string, vector<string>*>::iterator writing_iter[REDUCE_SLOT];
		string filepath;

		//long currentsize;
		long current_size[REDUCE_SLOT];


		int write_finish_cnt;
		bool is_write_finish[REDUCE_SLOT];

	public:
		iwriter(int ajobid, int anetworkidx);
		~iwriter();

		int get_jobid();
		//int get_numblock();
		int get_numblock(int hash_value);
		void add_keyvalue(string key, string value);
		bool is_writing(int hash_value);
		bool write_to_disk(int hash_value); // return true if write_to_disk should be stopped, return false if write should be continued
		bool IsFinish();
		bool IsWriteFinish(int hash_value);
		void flush(int hash_value);
};

iwriter::iwriter(int ajobid, int anetworkidx)
{
  write_buf = (char*)malloc(BUF_SIZE);
	jobid = ajobid;
	//iswriting = false;
	for (int i = 0; i < REDUCE_SLOT; ++i)
		is_writing_array[i] = false;
	networkidx = anetworkidx;
	//numblock = 0;
	for (int i = 0; i < REDUCE_SLOT; ++i)
		num_block[i] = 0;
	//writingblock = -1;
	for (int i = 0; i < REDUCE_SLOT; ++i)
		writing_block[i] = -1;
	//availableblock = -1; // until this index, write should be progressed
	for (int	i = 0; i < REDUCE_SLOT; ++i)
		available_block[i] = -1;
	//currentsize = 0;
	for (int i = 0; i < REDUCE_SLOT; ++i)
		current_size[i] = 0;
	//filefd = -1;
	for (int i = 0; i < REDUCE_SLOT; ++i)
		file_fd[i] = -1;
	//vectorindex = -1;
	for (int i = 0; i < REDUCE_SLOT; ++i)
		vector_index[i] = -1;
	//pos = -1;
	for (int i = 0; i < REDUCE_SLOT; ++i)
		position[i] = -1;

	// generate first map
	//themaps.push_back(new map<string, vector<string>*>);
	//keyvalues_maps = new vector<map<string, vector<string>*>*>(REDUCE_SLOT);
	for (int i = 0 ; i < REDUCE_SLOT; ++i)
		keyvalues_maps[i].push_back(new map<string, vector<string>*>);

	// we can increase the numblock because first write is guaranteed after the iwriter is initialized
	//numblock++;
	for (int i = 0; i < REDUCE_SLOT; ++i)
		num_block[i]++;

	write_finish_cnt = 0;
	for (int i = 0; i < REDUCE_SLOT; ++i)
		is_write_finish[i] = false;
}

iwriter::~iwriter()
{
}

int iwriter::get_jobid()
{
	return jobid;
}
/*
int iwriter::get_numblock()
{
	return numblock;
}
*/
int iwriter::get_numblock(int hash_value) {
	return num_block[hash_value];
}
void iwriter::add_keyvalue(string key, string value)
{
	// bsnam. check key	 key%NUM_CORES -> according to this, we should should which map will store this pair.
	// ie. themaps...-> we need to create	'num_cores' number of the maps.. 
	int hash_value; // [wb] split input files into number of reducers
	int key_integer = atoi(key.c_str());
	//hash_value = h(key.c_str(), HASHLENGTH);
	//hash_value = hash_value % REDUCE_SLOT;
	hash_value = key_integer % REDUCE_SLOT;
	if (hash_value < 0)
		hash_value = hash_value * (-1);

	vector<map<string, vector<string>*>*> &themaps = keyvalues_maps[hash_value];
	map<string, vector<string>*>::iterator &writing_it = writing_iter[hash_value];
	long &currentsize = current_size[hash_value];
	bool &iswriting = is_writing_array[hash_value];
	int &availableblock = available_block[hash_value]; // current available block
	int &writingblock = writing_block[hash_value]; // current writing block
	int &numblock = num_block[hash_value];
	int &vectorindex = vector_index[hash_value];
	int &pos = position[hash_value];
	int &filefd = file_fd[hash_value];


	pair<map<string, vector<string>*>::iterator, bool> ret;
	ret = themaps.back()->insert(pair<string, vector<string>*>(key, NULL));
            
	if(ret.second) // first key element
	{
		ret.first->second = new vector<string>;
		ret.first->second->push_back(value);

		currentsize += key.length() + value.length() + 2; // +2 for two '\n' character
	}
	else // key already exist
	{
		ret.first->second->push_back(value);

		currentsize += value.length() + 1; // +1 for '\n' character
	}

	// determine the new size
	if (currentsize > IBLOCKSIZE)
	{
		// new map generated
		themaps.push_back(new map<string, vector<string>*>);

		// trigger writing
		availableblock++;

		if (!iswriting) // if file is not open for written when it is avilable
		{
			writingblock++;

			// prepare iterator and indices
			writing_it = themaps[writingblock]->begin();
			vectorindex = 0;
			pos = 0;

			// determine the path of file
			filepath = DHT_PATH;
			stringstream ss;
			ss << ".job_";
			ss << jobid;
			ss << "_";
			ss << networkidx;
			ss << "_";
			ss << hash_value;
			ss << "_";
			ss << writingblock;
			filepath.append(ss.str());

			// open write file
			filefd = open(filepath.c_str(), O_APPEND|O_WRONLY|O_CREAT, 0644);

			if(filefd < 0)
				cout<<"[iwriter]Opening write file failed"<<endl;

			iswriting = true;
		}

		numblock++;
		currentsize = 0;
	}
}

bool iwriter::is_writing(int hash_value)
{
	bool &iswriting = is_writing_array[hash_value];
	return iswriting;
}

bool iwriter::write_to_disk(int hash_value) // return true if write_to_disk should be stopped, return false if write should be continued
{
	vector<map<string, vector<string>*>*> &themaps = keyvalues_maps[hash_value];
	map<string, vector<string>*>::iterator &writing_it = writing_iter[hash_value];
	int &writingblock = writing_block[hash_value]; // current writing block
	int &pos = position[hash_value];
	int &vectorindex = vector_index[hash_value];
	int &filefd = file_fd[hash_value];
	bool &iswriting = is_writing_array[hash_value];
	int &numblock = num_block[hash_value];
	int &availableblock = available_block[hash_value];

	while(pos < BUF_THRESHOLD && writing_it != themaps[writingblock]->end())
	{
		// append key and number of values if vectorindex is 0
		if(vectorindex == 0)
		{
			strcpy(write_buf + pos, writing_it->first.c_str());
			pos += writing_it->first.length();
			write_buf[pos] = '\n';
			pos++;

			stringstream ss;
			string message;
			ss << writing_it->second->size();
			message = ss.str();
			strcpy(write_buf + pos, message.c_str());
			pos += message.length();

			write_buf[pos] = '\n';
			pos++;
		}

		strcpy(write_buf + pos, (*writing_it->second)[vectorindex].c_str());
		pos += (*writing_it->second)[vectorindex].length();

		write_buf[pos] = '\n';
		pos++;

		vectorindex++;

		if((unsigned)vectorindex == writing_it->second->size())
		{
			writing_it++;
			vectorindex = 0;
		}
	}

	// write to the disk
	int ret = write(filefd, write_buf, pos);

	if(ret < 0)
	{
		cout<<"[iwriter]Writing to write file failed"<<endl;
	}

	pos = 0;

	// check if writing_it is themaps[writingblock]->end()
	if(writing_it == themaps[writingblock]->end()) // writing current block finished
	{
		// clear up current map, close open file and clear writing
		iswriting = false;
		close(filefd);
		filefd = -1;

		int free_size = 0;
		for(writing_it = themaps[writingblock]->begin(); writing_it != themaps[writingblock]->end(); writing_it++)
		{
			free_size += writing_it->second->size() * 19;
			delete writing_it->second;
		}
//printf("%d bytes have been freed from memory\n",free_size);

		vectorindex = 0;
		pos = 0;

		// check whether this block was the last block
		if(writingblock == numblock - 1)
		{
			// delete element of themaps
			for(int i = 0; (unsigned)i < themaps.size(); i++)
			{
				delete themaps[i];
			}

			write_finish_cnt++;
			is_write_finish[hash_value] = true;
			return true;
		}

		// check whether next block needs to be written
		if(writingblock < availableblock)
		{
			iswriting = true;
			writingblock++;

			// prepare iterator and indices
			writing_it = themaps[writingblock]->begin();
			vectorindex = 0;
			pos = 0;

			// determine the path of file
			filepath = DHT_PATH;
			stringstream ss;
			ss << ".job_";
			ss << jobid;
			ss << "_";
			ss << networkidx;
			ss << "_";
			ss << hash_value;
			ss << "_";
			ss << writingblock;
			filepath.append(ss.str());

			// open write file
			filefd = open(filepath.c_str(), O_APPEND|O_WRONLY|O_CREAT, 0644);

			if(filefd < 0)
				cout<<"[iwriter]Opening write file failed"<<endl;
		}
	}
	return false;
}

bool iwriter::IsWriteFinish(int hash_value) {
	return is_write_finish[hash_value];
}

bool iwriter::IsFinish() {
	if(write_finish_cnt == REDUCE_SLOT)
		return true;
	return false;
}

void iwriter::flush(int hash_value) // last call before iwriter is deleted. this is not an immediate flush()
{
	bool &iswriting = is_writing_array[hash_value];
	int &availableblock = available_block[hash_value];
	int &writingblock = writing_block[hash_value];
	int &vectorindex = vector_index[hash_value];
	int &pos = position[hash_value];
	//int &numblock = num_block[hash_value];
	map<string, vector<string>*>::iterator &writing_it = writing_iter[hash_value];
	vector<map<string, vector<string>*>*> &themaps = keyvalues_maps[hash_value];
	int &filefd = file_fd[hash_value];

	// let last block avilable for write
	availableblock++;

	if(!iswriting) // write was ongoing
	{
		iswriting = true;
		writingblock++;

		// prepare iterator and indices
		writing_it = themaps[writingblock]->begin();
		vectorindex = 0;
		pos = 0;

		// determine the path of file
		filepath = DHT_PATH;
		stringstream ss;
		ss << ".job_";
		ss << jobid;
		ss << "_";
		ss << networkidx;
		ss << "_";
		ss << hash_value;
		ss << "_";
		ss << writingblock;
		filepath.append(ss.str());

		// open write file
		filefd = open(filepath.c_str(), O_APPEND|O_WRONLY|O_CREAT, 0644);

		if(filefd < 0)
			cout<<"[iwriter]Opening write file failed"<<endl;
	}
}

#endif
