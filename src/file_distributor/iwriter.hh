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
    char write_buf[BUF_SIZE];
    bool iswriting;
    int jobid;
    int networkidx;
    int numblock;
    int filefd;
    int vectorindex;
    
    vector<map<string, vector<string>*>*> themaps;
    
    int writingblock;
    int availableblock;
    int pos; // position in the write_buf
    map<string, vector<string>*>::iterator writing_it;
    string filepath;
    
    long currentsize;
    
  public:
    iwriter (int ajobid, int anetworkidx);
    ~iwriter();
    
    int get_jobid();
    int get_numblock();
    void add_keyvalue (string key, string value);
    bool is_writing();
    bool write_to_disk(); // return true if write_to_disk should be stopped, return false if write should be continued
    void flush();
};

iwriter::iwriter (int ajobid, int anetworkidx)
{
  jobid = ajobid;
  iswriting = false;
  networkidx = anetworkidx;
  numblock = 0;
  writingblock = -1;
  availableblock = -1; // until this index, write should be progressed
  currentsize = 0;
  filefd = -1;
  vectorindex = -1;
  pos = -1;
  
  // generate first map
  themaps.push_back (new map<string, vector<string>*>);
  
  // we can increase the numblock because first write is guaranteed after the iwriter is initialized
  numblock++;
  
}

iwriter::~iwriter()
{
}

int iwriter::get_jobid()
{
  return jobid;
}
int iwriter::get_numblock()
{
  return numblock;
}

void iwriter::add_keyvalue (string key, string value)
{
  pair<map<string, vector<string>*>::iterator, bool> ret;
  
  ret = themaps.back()->insert (pair<string, vector<string>*> (key, NULL));
  
  if (ret.second)     // first key element
  {
    ret.first->second = new vector<string>;
    ret.first->second->push_back (value);
    
    currentsize += key.length() + value.length() + 2; // +2 for two '\n' character
    
  }
  
  else     // key already exist
  {
    ret.first->second->push_back (value);
    
    currentsize += value.length() + 1; // +1 for '\n' character
  }
  
  // determine the new size
  if (currentsize > IBLOCKSIZE)
  {
    // new map generated
    themaps.push_back (new map<string, vector<string>*>);
    
    // trigger writing
    availableblock++;
    
    if (!iswriting)     // if file is not open for written when it is avilable
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
      ss << writingblock;
      filepath.append (ss.str());
      
      // open write file
      filefd = open (filepath.c_str(), O_APPEND | O_WRONLY | O_CREAT, 0644);
      
      if (filefd < 0)
        cout << "[iwriter]Opening write file failed" << endl;
        
      iswriting = true;
    }
    
    numblock++;
    currentsize = 0;
  }
}

bool iwriter::is_writing()
{
  return iswriting;
}

bool iwriter::write_to_disk()   // return true if write_to_disk should be stopped, return false if write should be continued
{
  while (pos < BUF_THRESHOLD && writing_it != themaps[writingblock]->end())
  {
    // append key and number of values if vectorindex is 0
    if (vectorindex == 0)
    {
      strcpy (write_buf + pos, writing_it->first.c_str());
      pos += writing_it->first.length();
      write_buf[pos] = '\n';
      pos++;
      
      stringstream ss;
      string message;
      ss << writing_it->second->size();
      message = ss.str();
      strcpy (write_buf + pos, message.c_str());
      pos += message.length();
      
      write_buf[pos] = '\n';
      pos++;
    }
    
    strcpy (write_buf + pos, (*writing_it->second) [vectorindex].c_str());
    pos += (*writing_it->second) [vectorindex].length();
    
    write_buf[pos] = '\n';
    pos++;
    
    vectorindex++;
    
    if ( (unsigned) vectorindex == writing_it->second->size())
    {
      writing_it++;
      vectorindex = 0;
    }
  }
  
  // write to the disk
  int ret = write (filefd, write_buf, pos);
  
  if (ret < 0)
  {
    cout << "[iwriter]Writing to write file failed" << endl;
  }
  
  pos = 0;
  
  // check if writing_it is themaps[writingblock]->end()
  if (writing_it == themaps[writingblock]->end())     // writing current block finished
  {
    // clear up current map, close open file and clear writing
    iswriting = false;
    close (filefd);
    filefd = -1;
    
    for (writing_it = themaps[writingblock]->begin(); writing_it != themaps[writingblock]->end(); writing_it++)
    {
      delete writing_it->second;
    }
    
    vectorindex = 0;
    pos = 0;
    
    // check whether this block was the last block
    if (writingblock == numblock - 1)
    {
      // delete element of themaps
      for (int i = 0; (unsigned) i < themaps.size(); i++)
      {
        delete themaps[i];
      }
      
      return true;
    }
    
    // check whether next block needs to be written
    if (writingblock < availableblock)
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
      ss << writingblock;
      filepath.append (ss.str());
      
      // open write file
      filefd = open (filepath.c_str(), O_APPEND | O_WRONLY | O_CREAT, 0644);
      
      if (filefd < 0)
        cout << "[iwriter]Opening write file failed" << endl;
    }
  }
  
  return false;
}

void iwriter::flush()   // last call before iwriter is deleted. this is not an immediate flush()
{
  // let last block avilable for write
  availableblock++;
  
  if (!iswriting)     // write was ongoing
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
    ss << writingblock;
    filepath.append (ss.str());
    
    // open write file
    filefd = open (filepath.c_str(), O_APPEND | O_WRONLY | O_CREAT, 0644);
    
    if (filefd < 0)
      cout << "[iwriter]Opening write file failed" << endl;
  }
}

#endif
