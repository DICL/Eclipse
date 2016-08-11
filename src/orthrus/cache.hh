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
        size_t size;
        size_t capacity;
        vector<dataentry*> entries; // front() is the most recent entry, back() is the least recent entry
        vector<dataentry*> evicted;
        
    public:
        cache();
        cache (size_t num);
        dataentry* lookup (string filename);   // lookup makes the entry the most recent
        dataentry* lookup (unsigned index);   // lookup makes the entry the most recent
        
        void new_entry (dataentry* entry);   // a zero-sized entry is added as the most recent entry
        
        void update_size(); // this function should be called periodically
        size_t get_size();
        bool try_fit_size();
		size_t GetCapacity();
};

cache::cache()
{
    size = 0;
    capacity = CACHESIZE;
	capacity *= 1024*1024;
}

cache::cache (size_t num)
{
    size = 0;
    capacity = num;
	capacity *= 1024*1024;
}

dataentry* cache::lookup (string filename)
{
    dataentry* ret = NULL;

    /*
    cout << "lookup: " << filename << " from ";
    for (int i = 0; i < entries.size(); ++i) {
      cout << entries[i]->get_filename() << " ";
    }
    cout << endl;
    */
    
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
  bool haha;
  // haha = capacity > 0;
  haha = true;
	if (true) {
		entries.insert (entries.begin(), entry);
		//return true;
	}
	//else
	//	return false;
}

void cache::update_size()
{
    size = 0;
    
    for (int i = 0; (unsigned) i < entries.size(); i++)
    {
        size += entries[i]->get_size();
    }
    
    if (size > capacity)
    {
        try_fit_size();
    }
}

bool cache::try_fit_size()
{
    for (int i = 1; (unsigned) i <= entries.size(); i++)
    {
        dataentry* theentry = * (entries.end() - i);
        
        // if (!theentry->is_locked())
        {
            entries.erase ( (entries.end() - i));
            i--;
            size -= theentry->get_size();
            evicted.push_back(theentry);
            // delete theentry;
            
            if (size > capacity)
            {
                continue;
            }
            else
            {
                // return true;
                break;
            }
        }
    }
    for (int i = 0; i < evicted.size(); ++i) {
      auto theentry = evicted[i];
      if (!theentry->is_locked()) {
        evicted.erase(evicted.begin() + i);
        --i;
        delete theentry;
      }
    }
    
    // return false;
    return true;
}


size_t cache::get_size()
{
    return size;
}

size_t cache::GetCapacity() {
	return capacity;
}

#endif
