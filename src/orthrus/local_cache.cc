#include <local_cache.hh>
#include <algorithm>
#include <err.h>

using namespace orthrus;

// Constructor&getters/setters {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
Local_cache::Local_cache () {
 map_spatial = new Local_cache::MAP ();
 map_lru     = new Local_cache::MAP ();
 map_key	 = new Local_cache::SMAP ();
 size_current_bytes = 0;
 policy = NOTHING;

 pthread_mutex_init (&mutex_map_spatial, NULL);
 pthread_mutex_init (&mutex_map_lru, NULL);
 pthread_mutex_init (&mutex_queue_low, NULL);
 pthread_mutex_init (&mutex_queue_upp, NULL);
}
// ----------------------------------------------- 
Local_cache::~Local_cache () { 
 delete map_spatial; 
 delete map_key; 
 delete map_lru; 
}
// ----------------------------------------------- 
Local_cache& Local_cache::set_policy (int p) {
 policy = p;
 return *this;
}
// ----------------------------------------------- 
Local_cache& Local_cache::set_boundaries (uint64_t l, uint64_t u) {
 boundary_low = l;
 boundary_upp = u;
 return *this;
}
// ----------------------------------------------- 
Local_cache& Local_cache::set_ema (uint64_t e) {
 ema = e;
 return *this;
}
// ----------------------------------------------- 
Local_cache& Local_cache::set_size (size_t s) {
 size_max_bytes = s;
 return *this;
}
//}}}
// insert {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool Local_cache::insert (uint64_t idx, disk_page_t& dp) {
 auto it = map_spatial->find (idx);
 bool found_same       = (it != map_spatial->end());
 bool does_new_dp_fits = ((size_current_bytes + dp.get_size()) > size_max_bytes);
 dp.set_time (this->count++);

 if (does_new_dp_fits) { //! If its not full
  if (found_same) {
   if (policy & orthrus::LRU) {
    pthread_mutex_lock (&mutex_map_lru);
    map_lru->erase (it);
    map_lru->insert (*it);  // :TODO: //std::swap (it, map_lru->begin());
    pthread_mutex_unlock (&mutex_map_lru);
   }
  } else {
   pthread_mutex_lock (&mutex_map_spatial);
   map_spatial->insert (std::make_pair (idx, dp));
   pthread_mutex_unlock (&mutex_map_spatial);

   if (policy & orthrus::LRU) {
    pthread_mutex_lock (&mutex_map_lru);
    map_lru->insert (std::make_pair (idx, dp));
    pthread_mutex_unlock (&mutex_map_lru);
   }
  }
 } else {                //! If its full
  if (found_same) {
   if (policy & orthrus::LRU) {
    pthread_mutex_lock (&mutex_map_spatial);
    map_lru->erase (--map_lru->rbegin().base());
    map_lru->erase (idx);
    pthread_mutex_unlock (&mutex_map_spatial);
   // No need to update map_spatial
   }

  } else {
   pthread_mutex_lock (&mutex_map_spatial);
   map_spatial->insert (std::make_pair (idx, dp));
   pthread_mutex_unlock (&mutex_map_spatial);

   if (policy & orthrus::LRU) {
    pthread_mutex_lock (&mutex_map_lru);
    map_lru->insert (std::make_pair (idx, dp));
    pthread_mutex_unlock (&mutex_map_lru);
   }
   pop_farthest ();
  }
 } 

 size_current_bytes += dp.get_size();

 return true;
}

bool Local_cache::insert (string key, uint64_t idx, disk_page_t& dp)
{
	auto it = map_key->find(key);
	bool found_same = (it != map_key->end());
	bool does_new_dp_fits = ((size_current_bytes + dp.get_size()) > size_max_bytes);
	dp.set_time (this->count++);

	// if the disk page is larger than the cache size, return false
	if(size_max_bytes < dp.get_size())
		return false;

	if(does_new_dp_fits)
	{
		if(found_same)
		{
			if(policy & orthrus::LRU)
			{
				map_lru->erase(it);
				map_lru->insert(*it); // :TODO: //std::swap (it, map_lru->begin());
			}
		}
		else
		{
			map_spatial->insert(std::make_pair(idx, dp));
			map_key->insert(std::make_pair(key, dp));

			if(policy & orthrus::LRU)
			{
				map_lru->insert(std::make_pair(idx, dp));
			}
		}
	}
	else // if the cache is full
	{
		while()
			pop_oldest();
		if(found_same)
		{
			if(policy & orthrus::LRU)
			{
				map_lru->erase(--map_lru->begin().base());
				map_lru->erase(idx);
			}
		}
		else
		{
			map_spatial->insert (std::make_pair (idx, dp));

			if (policy & orthrus::LRU)
			{
				map_lru->insert (std::make_pair (idx, dp));
			}
			pop_farthest ();
		}
	}

	size_current_bytes += dp.get_size();

	return true;
}

//}}}
// lookup {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
disk_page_t Local_cache::lookup (uint64_t idx) throw (std::out_of_range) {
 auto victim = map_spatial->find (idx);
 if (victim != map_spatial->end ())  //! If it is found O(log n)
  return (*victim).second;
 else 
  throw std::out_of_range ("Not found");
}

disk_page_t* Local_cache::lookup (string)
{
}
//}}}
// Is_disk_page_belonging {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool Local_cache::is_disk_page_belonging (const disk_page_t& dp) {
 //uint64_t lowest   = map_spatial->begin()->second.get_index ();
 //uint64_t highest  = map_spatial->rbegin()->second.get_index ();
 //uint64_t max_dist = std::max (ema-lowest, highest-ema);
 //uint64_t oldest   = (*map_lru->begin()).get_time ();
 //return max_dist > (dp.get_index () - ema);
 return (boundary_low <= dp.get_index() and dp.get_index() <= boundary_upp);
}
//}}}
// get_local_center{{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
uint64_t Local_cache::get_local_center () {
 uint64_t counter = 0, total = 0;
 for (auto& dp : *map_spatial) {
  total += dp.second.get_index () * dp.second.get_size ();
  counter++;
 } 
 return total / counter;
}
// }}}
// pop_farthest {{{
// In case we exceed Delete the last page
// New policy, delete the darkest element :TRICKY:
// Complexity O(1)
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Local_cache::pop_farthest () {
 if (map_spatial->size () > size_max_bytes) { //! :FIXME:
  auto first = map_spatial->begin ();         //! 0(1)
  auto last  = map_spatial->rbegin ();        //! O(1)

  uint64_t lowest  = first->second.get_index ();
  uint64_t highest = last-> second.get_index ();

  //! If the victim belongs to the boundary of the node
  if (policy & SPATIAL) {
   if (boundary_low < lowest or highest < boundary_upp) {
    if ((ema - lowest) > (highest - ema)) { //! POP the leftest element
     pthread_mutex_lock (&mutex_queue_low);
     queue_lower.push (first->second);
     pthread_mutex_unlock (&mutex_queue_low);

     pthread_mutex_lock (&mutex_map_spatial);
     map_spatial->erase (first);
     pthread_mutex_unlock (&mutex_map_spatial);
 
     if (policy & orthrus::LRU) {
      pthread_mutex_lock (&mutex_map_lru);
      map_lru->erase (first->first);
      pthread_mutex_unlock (&mutex_map_lru);
     }

    } else if (highest > ema) { //! Pop the rightest element
     pthread_mutex_lock (&mutex_queue_upp);
     queue_upper.push (last->second);
     pthread_mutex_unlock (&mutex_queue_upp);

     pthread_mutex_lock (&mutex_map_spatial);
     map_spatial->erase (last->first);
     pthread_mutex_unlock (&mutex_map_spatial);

     if (policy & orthrus::LRU) {
      pthread_mutex_lock (&mutex_map_lru);
      map_lru->erase (last->first);
      pthread_mutex_unlock (&mutex_map_lru);
     }
    }
   }
// {{{
   //! Otherwise pop the oldest element :LRU:
//  } else ((policy & LRU) == LRU) {
//
//   set<disk_page_t>::iterator oldest = map_lru->begin();
//   uint64_t oldest_time = (*oldest).time;
//   uint64_t oldest_item = (*oldest).point;
//
//   //! Depends of the position
//   if (oldest_item < ema) {
//    pthread_mutex_lock (&mutex_queue_low);
//    queue_lower.push (*oldest);
//    pthread_mutex_unlock (&mutex_queue_low);
//
//   } else {
//    pthread_mutex_lock (&mutex_queue_upp);
//    queue_upper.push (*oldest);
//    pthread_mutex_unlock (&mutex_queue_upp);
//
//   }
//   pthread_mutex_lock (&mutex_map_spatial);
//   map_lru->erase (oldest_time);
//   map_spatial->erase (oldest_item);
//   pthread_mutex_unlock (&mutex_map_spatial);
//  }
// } 
// }}}
  }
 }
}
// }}}
// update {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Local_cache::boundaries_update (uint64_t low, uint64_t upp) {
 auto low_i = map_spatial->lower_bound (low);
 auto upp_i = map_spatial->upper_bound (upp);
 this->boundary_low = low;
 this->boundary_upp = upp;

 if (low_i != map_spatial->end() and low_i != map_spatial->begin()) {
  for_each (map_spatial->begin(), low_i, [&] (std::pair<uint64_t, disk_page_t> it) {
   pthread_mutex_lock (&mutex_queue_low);
   queue_lower.push (it.second);
   pthread_mutex_unlock (&mutex_queue_low);

   pthread_mutex_lock (&mutex_map_spatial);
   map_spatial->erase (it.first);
   pthread_mutex_unlock (&mutex_map_spatial);

   if (policy & orthrus::LRU) {
    pthread_mutex_lock (&mutex_map_lru);
    map_lru->erase (it.first);
    pthread_mutex_unlock (&mutex_map_lru);
   }
  });
 }  
 if (upp_i != map_spatial->end() and upp_i != map_spatial->begin())  {
  for_each (upp_i, map_spatial->end(), [&] (std::pair<uint64_t, disk_page_t> it) {
   pthread_mutex_lock (&mutex_queue_upp);
   queue_upper.push (it.second);
   pthread_mutex_unlock (&mutex_queue_upp);
 
   pthread_mutex_lock (&mutex_map_spatial);
   map_spatial->erase (it.first);
   pthread_mutex_unlock (&mutex_map_spatial);

   if (policy & orthrus::LRU) {
    pthread_mutex_lock (&mutex_map_lru);
    map_lru->erase (it.first);
    pthread_mutex_unlock (&mutex_map_lru);
   }
  });
 }
}
// }}}
// get_low / get_up {{{
// :FIXME: Optimize the return references
//                                -- Vicente Bolea
// ----------------------------------------------- 
disk_page_t Local_cache::get_low () {
 disk_page_t out = queue_lower.front ();
 pthread_mutex_lock (&mutex_queue_low);
 queue_lower.pop ();
 pthread_mutex_unlock (&mutex_queue_low);
 return out;
}
disk_page_t Local_cache::get_upp () {
 disk_page_t out = queue_upper.front ();
 pthread_mutex_lock (&mutex_queue_upp);
 queue_upper.pop ();
 pthread_mutex_unlock (&mutex_queue_upp);
 return out;
}
//}}}
