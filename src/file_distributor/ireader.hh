#ifndef __IREADER__
#define __IREADER__

#include <iostream>
#include <fstream>
#include <sstream>
#include "filepeer.hh"
#include "file_connclient.hh"
#include <mapreduce/definitions.hh>

using namespace std;

class ireader
{
  private:
    int jobid;
    int pos;
    int numiblock;
    int networkidx;
    int bridgeid;
    int readingfile;
    int finishedcount;
    
    string filename;
    filepeer* dstpeer;
    file_connclient* dstclient;
    
    char write_buf[BUF_SIZE];
    
    vector<ifstream*> files;
    vector<string> filepaths;
    vector<string> currentkeys;
    vector<int> remaining_record;
    
    string currentkey;
    
    map<string, vector<int>*> keyorder;
    
    bridgetype dsttype; // PEER, DISK, CACHE or CLIENT
    
    int get_jobid();
    int get_numiblock();
    file_connclient* get_dstclient();
    filepeer* get_dstpeer();
    void set_bridgeid (int anid);
    
  public:
    ireader (int ajobid, int anumiblock, int anetworkidx, int abridgeid, bridgetype adsttype);
    void set_dstpeer (filepeer* apeer);
    void set_dstclient (file_connclient* aclient);
    bool read_idata();
};

ireader::ireader (int ajobid, int anumiblock, int anetworkidx, int abridgeid, bridgetype adsttype)
{
  jobid = ajobid;
  numiblock = anumiblock;
  networkidx = anetworkidx;
  dstclient = NULL;
  dstpeer = NULL;
  readingfile = -1;
  finishedcount = 0;
  bridgeid = abridgeid;
  dsttype = adsttype;
  pos = 0;
  
  // prepare reading idata from multiple file block according to jobid and numiblock
  for (int i = 0; i < numiblock; i++)
  {
    string filename;
    stringstream ss;
    ss << DHT_PATH;
    ss << ".job_";
    ss << jobid;
    ss << "_";
    ss << networkidx;
    ss << "_";
    ss << i;
    filename = ss.str();
    
    files.push_back (new ifstream (filename.c_str()));
    filepaths.push_back (filename);
    
    // read first key and remaining record of the file
    string key;
    string remainstr;
    int remain;
    getline (*files[i], key);
    getline (*files[i], remainstr);
    remain = atoi (remainstr.c_str());
    currentkeys.push_back (key);
    remaining_record.push_back (remain);
    
    // insert into the keyorder map
    pair<map<string, vector<int>*>::iterator, bool> ret;
    ret = keyorder.insert (pair<string, vector<int>*> (key, NULL));
    
    if (ret.second)     // currently unique key
    {
      ret.first->second = new vector<int>;
      ret.first->second->push_back (i);
      
    }
    
    else     // key already exist
    {
      // add file index to the vector
      ret.first->second->push_back (i);
    }
  }
  
  // determine the first current key
  currentkey = keyorder.begin()->first;
  readingfile = keyorder.begin()->second->back();
  keyorder.begin()->second->pop_back();
  
  if (keyorder.begin()->second->size() == 0)
  {
    delete keyorder.begin()->second;
    keyorder.erase (keyorder.begin());
  }
  
  // prepare write_buf
  if (dsttype == CLIENT)
  {
    memset (write_buf, 0, BUF_SIZE);
    strcpy (write_buf + pos, currentkey.c_str());
    pos += currentkey.length();
    write_buf[pos] = '\n';
    pos++;
    
  }
  
  else
  {
    string bidstring;
    stringstream ss;
    ss << bridgeid;
    bidstring = ss.str();
    
    memset (write_buf, 0, BUF_SIZE);
    
    strcpy (write_buf + pos, bidstring.c_str());
    pos += bidstring.length();
    write_buf[pos] = '\n';
    pos++;
    strcpy (write_buf + pos, currentkey.c_str());
    pos += currentkey.length();
    write_buf[pos] = '\n';
    pos++;
  }
}

bool ireader::read_idata()
{
  while (pos < BUF_THRESHOLD)
  {
    // read one record
    string record;
    
    getline (*files[readingfile], record);
    remaining_record[readingfile]--;
    
    if (pos + record.length() >= BUF_SIZE)     // overflow if this record is added to the write_buf
    {
      if (dsttype == CLIENT)
      {
        // flush
        if (dstclient->msgbuf.size() > 1)
        {
          dstclient->msgbuf.back()->set_buffer (write_buf, dstclient->get_fd());
          dstclient->msgbuf.push_back (new messagebuffer());
          
        }
        
        else
        {
          if (nbwritebuf (dstclient->get_fd(),
                          write_buf, dstclient->msgbuf.back()) <= 0)
          {
            dstclient->msgbuf.push_back (new messagebuffer());
          }
        }
        
        // prepare another write_buf
        pos = 0;
        memset (write_buf, 0, BUF_SIZE);
        
      }
      
      else     // PEER
      {
        // flush
        if (dstpeer->msgbuf.size() > 1)
        {
          dstpeer->msgbuf.back()->set_buffer (write_buf, dstpeer->get_fd());
          dstpeer->msgbuf.push_back (new messagebuffer());
          
        }
        
        else
        {
          if (nbwritebuf (dstpeer->get_fd(),
                          write_buf, dstpeer->msgbuf.back()) <= 0)
          {
            dstpeer->msgbuf.push_back (new messagebuffer());
          }
        }
        
        // prepare another write_buf
        pos = 0;
        string bidstring;
        stringstream ss;
        ss << bridgeid;
        bidstring = ss.str();
        
        memset (write_buf, 0, BUF_SIZE);
        strcpy (write_buf + pos, bidstring.c_str());
        pos += bidstring.length();
        write_buf[pos] = '\n';
        pos++;
      }
      
      strcpy (write_buf + pos, record.c_str());
      pos += record.length();
      write_buf[pos] = '\n';
      pos++;
      
      return true;
    }
    
    strcpy (write_buf + pos, record.c_str());
    pos += record.length();
    write_buf[pos] = '\n';
    pos++;
    
    if (remaining_record[readingfile] == 0)     // [another file for same key] or [next key]
    {
      // read next key of the file
      string key;
      int remain;
      getline (*files[readingfile], key);
      
      if (files[readingfile]->eof())     // no more key value for this file
      {
        // clear this file
        files[readingfile]->close();
        delete files[readingfile];
        finishedcount++;
        
        if (files.size() == (unsigned) finishedcount)       // all key value are sent
        {
          if (dsttype == CLIENT)
          {
            // flush current record
            if (dstclient->msgbuf.size() > 1)
            {
              dstclient->msgbuf.back()->set_buffer (write_buf, dstclient->get_fd());
              dstclient->msgbuf.push_back (new messagebuffer());
              
            }
            
            else
            {
              if (nbwritebuf (dstclient->get_fd(),
                              write_buf, dstclient->msgbuf.back()) <= 0)
              {
                dstclient->msgbuf.push_back (new messagebuffer());
              }
            }
            
            // notify target the end of idata(send zero packet twice)
            pos = 0;
            memset (write_buf, 0, BUF_SIZE);
            
            if (dstclient->msgbuf.size() > 1)
            {
              dstclient->msgbuf.back()->set_buffer (write_buf, dstclient->get_fd());
              dstclient->msgbuf.push_back (new messagebuffer());
              
            }
            
            else
            {
              if (nbwritebuf (dstclient->get_fd(),
                              write_buf, dstclient->msgbuf.back()) <= 0)
              {
                dstclient->msgbuf.push_back (new messagebuffer());
              }
            }
            
            if (dstclient->msgbuf.size() > 1)
            {
              dstclient->msgbuf.back()->set_buffer (write_buf, dstclient->get_fd());
              dstclient->msgbuf.push_back (new messagebuffer());
              
            }
            
            else
            {
              if (nbwritebuf (dstclient->get_fd(),
                              write_buf, dstclient->msgbuf.back()) <= 0)
              {
                dstclient->msgbuf.push_back (new messagebuffer());
              }
            }
            
          }
          
          else     // PEER
          {
            // flush current record
            if (dstpeer->msgbuf.size() > 1)
            {
              dstpeer->msgbuf.back()->set_buffer (write_buf, dstpeer->get_fd());
              dstpeer->msgbuf.push_back (new messagebuffer());
              
            }
            
            else
            {
              if (nbwritebuf (dstpeer->get_fd(),
                              write_buf, dstpeer->msgbuf.back()) <= 0)
              {
                dstpeer->msgbuf.push_back (new messagebuffer());
              }
            }
            
            // notify target the end of idata(send zero packet twice)
            pos = 0;
            string bidstring;
            stringstream ss;
            ss << bridgeid;
            bidstring = ss.str();
            
            memset (write_buf, 0, BUF_SIZE);
            strcpy (write_buf + pos, bidstring.c_str());
            pos += bidstring.length();
            write_buf[pos] = '\n';
            
            if (dstpeer->msgbuf.size() > 1)
            {
              dstpeer->msgbuf.back()->set_buffer (write_buf, dstpeer->get_fd());
              dstpeer->msgbuf.push_back (new messagebuffer());
              
            }
            
            else
            {
              if (nbwritebuf (dstpeer->get_fd(),
                              write_buf, dstpeer->msgbuf.back()) <= 0)
              {
                dstpeer->msgbuf.push_back (new messagebuffer());
              }
            }
            
            if (dstpeer->msgbuf.size() > 1)
            {
              dstpeer->msgbuf.back()->set_buffer (write_buf, dstpeer->get_fd());
              dstpeer->msgbuf.push_back (new messagebuffer());
              
            }
            
            else
            {
              if (nbwritebuf (dstpeer->get_fd(),
                              write_buf, dstpeer->msgbuf.back()) <= 0)
              {
                dstpeer->msgbuf.push_back (new messagebuffer());
              }
            }
          }
          
          // return false to notify the end of idata to fileserver
          return false;
        }
        
      }
      
      else     // another key in this file
      {
      
        // register another key of this file to the map
        string remainstr;
        pair<map<string, vector<int>*>::iterator, bool> ret;
        ret = keyorder.insert (pair<string, vector<int>*> (key, NULL));
        
        if (ret.second)     // currently unique key
        {
          ret.first->second = new vector<int>;
          ret.first->second->push_back (readingfile);
          
        }
        
        else     // key already exist
        {
          // add file index to the vector
          ret.first->second->push_back (readingfile);
        }
        
        getline (*files[readingfile], remainstr);
        remain = atoi (remainstr.c_str());
        
        remaining_record[readingfile] = remain;
      }
      
      // determine next key(same or different from current key)
      if (currentkey == keyorder.begin()->first)     // another file for same key
      {
        readingfile = keyorder.begin()->second->back();
        keyorder.begin()->second->pop_back();
        
        if (keyorder.begin()->second->size() == 0)
        {
          delete keyorder.begin()->second;
          keyorder.erase (keyorder.begin());
        }
        
      }
      
      else     // different key
      {
        currentkey = keyorder.begin()->first;
        readingfile = keyorder.begin()->second->back();
        keyorder.begin()->second->pop_back();
        
        if (keyorder.begin()->second->size() == 0)
        {
          delete keyorder.begin()->second;
          keyorder.erase (keyorder.begin());
        }
        
        if (dsttype == CLIENT)
        {
          // flush
          if (dstclient->msgbuf.size() > 1)
          {
            dstclient->msgbuf.back()->set_buffer (write_buf, dstclient->get_fd());
            dstclient->msgbuf.push_back (new messagebuffer());
            
          }
          
          else
          {
            if (nbwritebuf (dstclient->get_fd(),
                            write_buf, dstclient->msgbuf.back()) <= 0)
            {
              dstclient->msgbuf.push_back (new messagebuffer());
            }
          }
          
          // prepare another write_buf
          pos = 0;
          memset (write_buf, 0, BUF_SIZE);
          
          // notify target end of key
          if (dstclient->msgbuf.size() > 1)
          {
            dstclient->msgbuf.back()->set_buffer (write_buf, dstclient->get_fd());
            dstclient->msgbuf.push_back (new messagebuffer());
            
          }
          
          else
          {
            if (nbwritebuf (dstclient->get_fd(),
                            write_buf, dstclient->msgbuf.back()) <= 0)
            {
              dstclient->msgbuf.push_back (new messagebuffer());
            }
          }
          
        }
        
        else     // PEER
        {
          // flush
          if (dstpeer->msgbuf.size() > 1)
          {
            dstpeer->msgbuf.back()->set_buffer (write_buf, dstpeer->get_fd());
            dstpeer->msgbuf.push_back (new messagebuffer());
            
          }
          
          else
          {
            if (nbwritebuf (dstpeer->get_fd(),
                            write_buf, dstpeer->msgbuf.back()) <= 0)
            {
              dstpeer->msgbuf.push_back (new messagebuffer());
            }
          }
          
          // prepare another write_buf
          pos = 0;
          string bidstring;
          stringstream ss;
          ss << bridgeid;
          bidstring = ss.str();
          
          memset (write_buf, 0, BUF_SIZE);
          strcpy (write_buf + pos, bidstring.c_str());
          pos += bidstring.length();
          write_buf[pos] = '\n';
          pos++;
          
          // notify target end of key
          if (dstpeer->msgbuf.size() > 1)
          {
            dstpeer->msgbuf.back()->set_buffer (write_buf, dstpeer->get_fd());
            dstpeer->msgbuf.push_back (new messagebuffer());
            
          }
          
          else
          {
            if (nbwritebuf (dstpeer->get_fd(),
                            write_buf, dstpeer->msgbuf.back()) <= 0)
            {
              dstpeer->msgbuf.push_back (new messagebuffer());
            }
          }
        }
        
        // add key to the write_buf
        strcpy (write_buf + pos, currentkey.c_str());
        pos += currentkey.length();
        write_buf[pos] = '\n';
        pos++;
        
        return true;
      }
    }
  }
  
  // write to the target
  if (dsttype == CLIENT)
  {
    // flush
    if (dstclient->msgbuf.size() > 1)
    {
      dstclient->msgbuf.back()->set_buffer (write_buf, dstclient->get_fd());
      dstclient->msgbuf.push_back (new messagebuffer());
      
    }
    
    else
    {
      if (nbwritebuf (dstclient->get_fd(),
                      write_buf, dstclient->msgbuf.back()) <= 0)
      {
        dstclient->msgbuf.push_back (new messagebuffer());
      }
    }
    
    // prepare another write_buf
    pos = 0;
    memset (write_buf, 0, BUF_SIZE);
    
  }
  
  else     // PEER
  {
    // flush
    if (dstpeer->msgbuf.size() > 1)
    {
      dstpeer->msgbuf.back()->set_buffer (write_buf, dstpeer->get_fd());
      dstpeer->msgbuf.push_back (new messagebuffer());
      
    }
    
    else
    {
      if (nbwritebuf (dstpeer->get_fd(),
                      write_buf, dstpeer->msgbuf.back()) <= 0)
      {
        dstpeer->msgbuf.push_back (new messagebuffer());
      }
    }
    
    // prepare another write_buf
    pos = 0;
    string bidstring;
    stringstream ss;
    ss << bridgeid;
    bidstring = ss.str();
    
    memset (write_buf, 0, BUF_SIZE);
    strcpy (write_buf + pos, bidstring.c_str());
    pos += bidstring.length();
    write_buf[pos] = '\n';
    pos++;
  }
  
  return true;
}

file_connclient* ireader::get_dstclient()
{
  return dstclient;
}

filepeer* ireader::get_dstpeer()
{
  return dstpeer;
}

int ireader::get_jobid()
{
  return jobid;
}

int ireader::get_numiblock()
{
  return numiblock;
}

void ireader::set_bridgeid (int anid)
{
  bridgeid = anid;
}

void ireader::set_dstpeer (filepeer* apeer)
{
  dsttype = PEER;
  dstpeer = apeer;
}

void ireader::set_dstclient (file_connclient* aclient)
{
  dsttype = CLIENT;
  dstclient = aclient;
}

#endif
