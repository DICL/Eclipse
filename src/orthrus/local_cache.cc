#include <local_cache.hh>
#include <algorithm>
#include <err.h>

// Constructor {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
Local_cache::Local_cache (int _size) {
 this->cache = new set<diskPage, bool (*) (const diskPage&, const diskPage&)> (diskPage::less_than);
 this->cache_time = new set<diskPage, bool (*) (const diskPage&, const diskPage&)> (diskPage::less_than_lru);
 this->_max = _size;
 this->policy = NOTHING;

 pthread_mutex_init (&mutex_cache,     NULL);
 pthread_mutex_init (&mutex_queue_low, NULL);
 pthread_mutex_init (&mutex_queue_upp, NULL);
}
//}}}
// insert {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool Local_cache::insert (diskPage& dp) {
 dp.time = this->count++;

 auto it = cache->find (dp);
 bool found_same = (it != cache->end());

 if (count < (size_t) _max) { //! If its not full
  if (found_same) {
   pthread_mutex_lock (&mutex_cache);
   cache_time->erase (*it);
   cache_time->insert (*it);  // :TODO:
   //std::swap (it, cache_time->begin());
   pthread_mutex_unlock (&mutex_cache);

  } else {
   pthread_mutex_lock (&mutex_cache);
   cache->insert (dp);
   cache_time->insert (dp);
   pthread_mutex_unlock (&mutex_cache);
  }
 } else {                     //! If its full
  if (found_same) {
   pthread_mutex_lock (&mutex_cache);
   cache_time->erase (dp);
   cache_time->erase (dp);
   pthread_mutex_unlock (&mutex_cache);
   // No need to update cache

  } else {
   pthread_mutex_lock (&mutex_cache);
   cache->insert (dp);
   cache_time->insert (dp);
   pthread_mutex_unlock (&mutex_cache);
   pop_farthest ();
  }
 } 
 return true;
}
//}}}
// lookup {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
diskPage Local_cache::lookup (uint64_t idx) throw (std::out_of_range) {
 diskPage a (idx);
 auto victim = cache->find (a);
 if (victim != cache->end ())  //! If it is found O(log n)
  return *victim;
 else 
  throw std::out_of_range ("Not found");
}
//}}}
// is_valid {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool Local_cache::is_valid (diskPage& dp) {
 uint64_t max_dist;
 auto first = cache->begin (); //! 0(1)
 auto last = cache->rbegin (); //! O(1)
 auto old = cache_time->begin (); //! 0(1)

 uint64_t lowest  = (*first).point;
 uint64_t highest = (*last).point;
 uint64_t oldest = (*old).time;

 if ((uint64_t)ema - lowest > (uint64_t)highest - ema)
  max_dist = labs (((uint64_t)ema) - lowest);

 else 
  max_dist = labs ((uint64_t)(((uint64_t)highest) - ema));

 //! If the new DP was more recently used than the oldest :LRU:
 if (dp.time < oldest || 
     max_dist > (uint64_t)(labs ( (uint64_t) ((uint64_t)dp.point - ((uint64_t)ema)) ))) {
  diskPage in = dp;

  pthread_mutex_lock (&mutex_cache);
  cache->insert (in);
  cache_time->insert (in);
  pthread_mutex_unlock (&mutex_cache);
  pop_farthest ();

  return true;
 }
 return false;
}
//}}}
// pop_farthest {{{
// In case we exceed Delete the last page
// New policy, delete the darkest element :TRICKY:
// Complexity O(1)
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Local_cache::pop_farthest () {
 if ((int)cache->size () > this->_max) {
  set<diskPage>::iterator first = cache->begin (); //! 0(1)
  set<diskPage>::reverse_iterator last = cache->rbegin (); //! O(1)

  uint64_t lowest  = (*first).point;
  uint64_t highest = (*last).point;

  //! If the victim belongs to the boundary of the node
  if ((policy & SPATIAL) == SPATIAL) {
   if (boundary_low < lowest || highest < boundary_upp) {
    //! POP the leftend element
    if (((uint64_t)ema - lowest) > ((uint64_t)highest - ema)) {

     pthread_mutex_lock (&mutex_queue_low);
     queue_lower.push (*first);
     pthread_mutex_unlock (&mutex_queue_low);

     pthread_mutex_lock (&mutex_cache);
     cache->erase (*first);
     cache_time->erase (*first);
     pthread_mutex_unlock (&mutex_cache);

     //! Pop the rightend element
    } else if ((uint64_t)highest > ema) { 

     pthread_mutex_lock (&mutex_queue_upp);
     queue_upper.push (*last);
     pthread_mutex_unlock (&mutex_queue_upp);

     pthread_mutex_lock (&mutex_cache);
     cache->erase (*last);
     cache_time->erase (*last);
     pthread_mutex_unlock (&mutex_cache);
    }
   }
   //! Otherwise pop the oldest element :LRU:
//  } else ((policy & LRU) == LRU) {
//
//   set<diskPage>::iterator oldest = cache_time->begin();
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
//   pthread_mutex_lock (&mutex_cache);
//   cache_time->erase (oldest_time);
//   cache->erase (oldest_item);
//   pthread_mutex_unlock (&mutex_cache);
//  }
// }
  }
 }
}
// }}}
// update {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Local_cache::update (double low, double upp) {
 auto low_i = cache->lower_bound (diskPage ((uint64_t)(low + .5)));
 auto upp_i = cache->upper_bound (diskPage ((uint64_t)(upp + .5)));

 if (!(low_i == cache->end ()) && !(low_i == cache->begin ())) {
  for_each (cache->begin(), low_it, [&] (auto it) {
   pthread_mutex_lock (&mutex_queue_low);
   queue_lower.push (*it);
   pthread_mutex_unlock (&mutex_queue_low);

   pthread_mutex_lock (&mutex_cache);
   cache->erase (it);
   pthread_mutex_unlock (&mutex_cache);
  });
 }  

 if (!(upp_i == cache->end ()) && !(upp_i == cache->begin ()))  {
  for_each (upp_i, cache->end(), [&] (auto it) {
   pthread_mutex_lock (&mutex_queue_upp);
   queue_upper.push (*it);
   pthread_mutex_unlock (&mutex_queue_upp);
 
   pthread_mutex_lock (&mutex_cache);
   cache->erase (it);
   pthread_mutex_unlock (&mutex_cache);
 });
}

diskPage Local_cache::get_diskPage (uint64_t idx) {
 diskPage a (idx);
 set<diskPage>::iterator victim = cache->find (a);

 if (victim != cache->end ()) {  //! If it is found O(log n)
  return *victim;

 } else {

  long currentChunk = (a.point* DPSIZE); //! read a block from a file
  ifstream file (path, ios::in | ios::binary);

  if (!file.good ()) { perror ("FILE NOT FOUND"); exit (EXIT_FAILURE); } 

  file.seekg (currentChunk, ios_base::beg);
  file.read (a.chunk, DPSIZE);
  file.close (); 

  return a;
 }
}

//}}}
// get_low / get_up {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
diskPage Local_cache::get_low () {
 diskPage out = queue_lower.front ();
 pthread_mutex_lock (&mutex_queue_low);
 queue_lower.pop ();
 pthread_mutex_unlock (&mutex_queue_low);
 return out;
}
diskPage Local_cache::get_upp () {
 diskPage out = queue_upper.front ();
 pthread_mutex_lock (&mutex_queue_upp);
 queue_upper.pop ();
 pthread_mutex_unlock (&mutex_queue_upp);
 return out;
}
//}}}
