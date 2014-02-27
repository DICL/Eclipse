#include <local_map_spatial.hh>
#include <algorithm>
#include <err.h>

using namespace orthrus;

// Constructor {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
Local_map_spatial::Local_map_spatial (int _size) {
 map_spatial = new MAP ();
 map_lru     = new MAP ();
 size_max_bytes = _size;
 size_current_bytes = 0;
 policy = NOTHING;

 pthread_mutex_init (&mutex_map_spatial, NULL);
 pthread_mutex_init (&mutex_map_lru, NULL);
 pthread_mutex_init (&mutex_queue_low, NULL);
 pthread_mutex_init (&mutex_queue_upp, NULL);
}
//
Local_map_spatial::~Local_cache () { 
 delete map_spatial; 
 delete map_lru; 
}
//}}}
// insert {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool Local_map_spatial::insert (uint64_t idx, disk_page_t& dp) {
 auto it = map_spatial->find (idx);
 bool found_same       = (it != map_spatial->end());
 bool does_new_dp_fits = ((size_current_bytes + dp.get_size()) > size_max_bytes);
 dp.time = this->count++;

 if (does_new_dp_fits) { //! If its not full
  if (found_same) {
   pthread_mutex_lock (&mutex_map_spatial);
   map_lru->erase (*it);
   map_lru->insert (*it);  // :TODO: //std::swap (it, map_lru->begin());
   pthread_mutex_unlock (&mutex_map_spatial);

  } else {
   pthread_mutex_lock (&mutex_map_spatial);
   map_spatial->insert (std::make_pair (idx, dp));
   pthread_mutex_unlock (&mutex_map_spatial);

   pthread_mutex_lock (&mutex_map_lru);
   map_lru->insert (std::make_pair (idx, dp));
   pthread_mutex_unlock (&mutex_map_lru);
  }
 } else {                //! If its full
  if (found_same) {
   pthread_mutex_lock (&mutex_map_spatial);
   map_lru->erase (map_lru->rbegin());
   map_lru->erase (dp);
   pthread_mutex_unlock (&mutex_map_spatial);
   // No need to update map_spatial

  } else {
   pthread_mutex_lock (&mutex_map_spatial);
   map_spatial->insert (dp);
   map_lru->insert (dp);
   pthread_mutex_unlock (&mutex_map_spatial);
   pop_farthest ();
  }
 } 
 return true;
}
//}}}
// lookup {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
disk_page_t Local_map_spatial::lookup (uint64_t idx) throw (std::out_of_range) {
 auto victim = map_spatial->find (idx);
 if (victim != map_spatial->end ())  //! If it is found O(log n)
  return (*victim).second;
 else 
  throw std::out_of_range ("Not found");
}
//}}}
// Is_disk_page_belonging {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool Local_map_spatial::is_disk_page_belonging (disk_page_t& dp) {
 uint64_t lowest   = (*map->begin()).get_index ();
 uint64_t highest  = (*map->rbegin()).get_index ();
 uint64_t max_dist = std::max (ema-lowest, highest-ema);
 //uint64_t oldest   = (*map_lru->begin()).get_time ();

 return dp.time < oldest || max_dist > (dp.get_index () - ema);
}
//}}}
// get_local_center{{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
uint32_t get_local_center () {
 uint32_t counter = 0, total = 0;
 for (auto& dp : map_spatial) {
  total += dp.get_index () * dp.get_size ();
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
void Local_map_spatial::pop_farthest () {
 if (map_spatial->size () > size_max_bytes) { //! :FIXME:
  auto first = map_spatial->begin ();         //! 0(1)
  auto last  = map_spatial->rbegin ();        //! O(1)

  uint64_t lowest  = (*first).point;
  uint64_t highest = (*last).point;

  //! If the victim belongs to the boundary of the node
  if (policy & SPATIAL) {
   if (boundary_low < lowest || highest < boundary_upp) {
    if ((ema - lowest) > (highest - ema)) { //! POP the leftest element
     pthread_mutex_lock (&mutex_queue_low);
     queue_lower.push (*first);
     pthread_mutex_unlock (&mutex_queue_low);

     pthread_mutex_lock (&mutex_map_spatial);
     map_spatial->erase (*first);
     map_lru->erase (*first);
     pthread_mutex_unlock (&mutex_map_spatial);

    } else if (highest > ema) { //! Pop the rightest element
     pthread_mutex_lock (&mutex_queue_upp);
     queue_upper.push (*last);
     pthread_mutex_unlock (&mutex_queue_upp);

     pthread_mutex_lock (&mutex_map_spatial);
     map_spatial->erase (*last);
     map_lru->erase (*last);
     pthread_mutex_unlock (&mutex_map_spatial);
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
void Local_map_spatial::boundaries_update (uint64_t low, uint64_t upp) {
 auto low_i = map_spatial->lower_bound (low);
 auto upp_i = map_spatial->upper_bound (upp);

 if (low_i != map_spatial->end()  &&  low_i != map_spatial->begin()) {
  for_each (map_spatial->begin(), low_it, [&] (auto it) {
   pthread_mutex_lock (&mutex_queue_low);
   queue_lower.push (*it);
   pthread_mutex_unlock (&mutex_queue_low);

   pthread_mutex_lock (&mutex_map_spatial);
   map_spatial->erase (it);
   pthread_mutex_unlock (&mutex_map_spatial);
  });
 }  
 if (upp_i != map_spatial->end()  &&  upp_i != map_spatial->begin())  {
  for_each (upp_i, map_spatial->end(), [&] (auto it) {
   pthread_mutex_lock (&mutex_queue_upp);
   queue_upper.push (*it);
   pthread_mutex_unlock (&mutex_queue_upp);
 
   pthread_mutex_lock (&mutex_map_spatial);
   map_spatial->erase (it);
   pthread_mutex_unlock (&mutex_map_spatial);
  });
 }
}
// }}}
// get_low / get_up {{{
// :FIXME: Optimize the return references
//                                -- Vicente Bolea
// ----------------------------------------------- 
disk_page_t Local_map_spatial::get_low () {
 disk_page_t out = queue_lower.front ();
 pthread_mutex_lock (&mutex_queue_low);
 queue_lower.pop ();
 pthread_mutex_unlock (&mutex_queue_low);
 return out;
}
disk_page_t Local_map_spatial::get_upp () {
 disk_page_t out = queue_upper.front ();
 pthread_mutex_lock (&mutex_queue_upp);
 queue_upper.pop ();
 pthread_mutex_unlock (&mutex_queue_upp);
 return out;
}
//}}}
