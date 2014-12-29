#ifndef __CACHE__
#define __CACHE__

#include <iostream>
#include <orthrus/dataentry.hh>
#include <orthrus/datablock.hh>

using namespace std;

// lookup: check_being_written() should be explicitly called from a caller
// update_size() should be periodically called from super class

class cache
{
  private:
    unsigned size;
    unsigned capacity;
    vector<dataentry*> entries; // front() is the most recent entry, back() is the least recent entry
    
  public:
    cache();
    cache (unsigned num);
    dataentry* lookup (string filename);   // lookup makes the entry the most recent
    dataentry* lookup (unsigned index);   // lookup makes the entry the most recent
    
    void new_entry (dataentry* entry);   // a zero-sized entry is added as the most recent entry
    
    void update_size(); // this function should be called periodically
    unsigned get_size();
    bool try_fit_size();
};

cache::cache()
{
  size = 0;
  capacity = CACHESIZE;
}

cache::cache (unsigned num)
{
  size = 0;
  capacity = num;
}

dataentry* cache::lookup (string filename)
{
  dataentry* ret = NULL;
  
  for (int i = 0; (unsigned) i < entries.size(); i++)
  {
    if (filename == entries[i]->get_filename())
    {
      ret = entries[i];
      // make the entry the most recent
      entries.erase (entries.begin() + i);
      entries.insert (entries.begin(), ret);
      return ret;
    }
  }
  
  return NULL;
}

dataentry* cache::lookup (unsigned index)
{
  dataentry* ret = NULL;
  
  for (int i = 0; (unsigned) i < entries.size(); i++)
  {
    if (index == entries[i]->get_index())
    {
      ret = entries[i];
      // make the entry the most recent
      entries.erase (entries.begin() + i);
      entries.insert (entries.begin(), ret);
      return ret;
    }
  }
  
  return NULL;
}

void cache::new_entry (dataentry* entry)
{
  entries.insert (entries.begin(), entry);
}

void cache::update_size()
{
  size = 0;
  
  for (int i = 0; (unsigned) i < entries.size(); i++)
    size += entries[i]->get_size();
    
  if (size > capacity)
    try_fit_size();
}

bool cache::try_fit_size()
{
  for (int i = 1; (unsigned) i <= entries.size(); i++)
  {
    dataentry* theentry = * (entries.end() - i);
    
    if (!theentry->is_locked())
    {
      entries.erase ( (entries.end() - i));
      i--;
      size -= theentry->get_size();
      delete theentry;
      
      if (size > capacity)
      {
        continue;
      }
      else
      {
        return true;
      }
    }
  }
  
  return false;
}


unsigned cache::get_size()
{
  return size;
}


#endif
