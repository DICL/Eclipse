#include <SETcache.hh>
#include <err.h>

// Join {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void join (uint64_t item) {

 uint64_t token1_A, token2_A, token1_B;
 string token3_A, token3_B;
 ifstream tableA, tableB;

 tableA.open ("/home/vicente/simring/table_a.dat", ifstream::in);
 tableB.open ("/home/vicente/simring/table_b.dat", ifstream::in);

 if (tableA.fail () || tableB.fail ()) err (EXIT_FAILURE, "no tables found");

 //! Search item in tableA
 while (!tableA.eof () && (token1_A != item)) {

  tableA >> token1_A >> token2_A >> token3_A;

  //! Search all the queries in tableB which are same as that query in tableA
  while (!tableB.eof () && (token1_B != token1_A)) {
   tableB >> token1_B >> token3_B;
  }
 }

 tableA.close();
 tableB.close();
}
//}}}
// Constructor {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
SETcache::SETcache (int _size, char* p) {
 this->cache = new set<diskPage, bool (*) (const diskPage&, const diskPage&)> (diskPage::less_than);
 this->cache_time = new set<diskPage, bool (*) (const diskPage&, const diskPage&)> (diskPage::less_than_lru);
 this->_max = _size;
 this->policy = NOTHING;

 if (p != NULL) setDataFile (p);

 pthread_mutex_init (&mutex_match,     NULL);
 pthread_mutex_init (&mutex_queue_low, NULL);
 pthread_mutex_init (&mutex_queue_upp, NULL);
}
//}}}
// setDataFile {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void SETcache::setDataFile (char* p) { 
 strncpy (this->path, p, 256);
}
//}}}
// operator<< {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
ostream& operator<< (ostream& out, SETcache& in) {
 set<diskPage>::iterator it; 
 out << "Size: [" << in.cache->size() << "]" << endl;
 out << "Current elements in the cache" << endl;
 out << "---------------------------" << endl;
 out << "SET: ";
 for (it = in.cache->begin (); it != in.cache->end(); it++)
  out << "Item: [" << (*it).point << "] , ";
 out << endl;
 if (!in.queue_lower.empty()) {
  out << "QUEUE_LOW: [FRONT=" << in.queue_lower.front().point;
  out << "] [BACK=" << in.queue_lower.back().point << "]" << endl; 
 } 

 if (!in.queue_upper.empty()) {
  out << "QUEUE_LOW: [FRONT=" << in.queue_upper.front().point;
  out << "] [BACK=" << in.queue_upper.back().point << "]" << endl; 
 }
 out << "---------------------------" << endl;
 return out;
}
//}}}
// match {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool SETcache::match (Query& q) {
// if (policy & UPDATE) update (q.low_b, q.upp_b);

 this->boundary_low = q.low_b;
 this->boundary_upp = q.upp_b;
 this->ema = q.EMA;

 diskPage a (q.point);
 a.time = q.time;

 set<diskPage>::iterator victim = cache->find (a);

 if (victim != cache->end ()) {  //! If it is found O(log n)
  pthread_mutex_lock (&mutex_match);
  cache_time->erase (*victim);
  cache_time->insert (a);
  pthread_mutex_unlock (&mutex_match);

  return true;

 } else {

  long currentChunk = (a.point * DPSIZE); //! read a block from a file
  ifstream file (path, ios::in | ios::binary);

  if (!file.good ()) { perror ("FILE NOT FOUND"); exit (EXIT_FAILURE); } 

  file.seekg (currentChunk, ios_base::beg);
  file.read (a.chunk, DPSIZE);
  file.close (); 

  if (policy & JOIN) join (10000);

  //! Inserting [ O(logn) ]
  pthread_mutex_lock (&mutex_match);
  cache->insert (a);
  cache_time->insert (a);
  pthread_mutex_unlock (&mutex_match);

  pop_farthest ();
  return false;
 }
}
//}}}
// get_low {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
diskPage SETcache::get_low () {

 diskPage out = queue_lower.front ();
 pthread_mutex_lock (&mutex_queue_low);
 queue_lower.pop ();
 pthread_mutex_unlock (&mutex_queue_low);
 return out;
}
//}}}
// get_upp {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
diskPage SETcache::get_upp () {

 diskPage out = queue_upper.front ();
 pthread_mutex_lock (&mutex_queue_upp);
 queue_upper.pop ();
 pthread_mutex_unlock (&mutex_queue_upp);
 return out;
}
//}}}
// is_valid {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool SETcache::is_valid (diskPage& dp) {
 //uint64_t item (dp.index);
 uint64_t max_dist;

 //! 2st test: Is not the farthest one 
 set<diskPage>::iterator first = cache->begin (); //! 0(1)
 set<diskPage>::reverse_iterator last = cache->rbegin (); //! O(1)
 set<diskPage>::iterator old = cache_time->begin (); //! 0(1)

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

  pthread_mutex_lock (&mutex_match);
  cache->insert (in);
  cache_time->insert (in);
  pthread_mutex_unlock (&mutex_match);
  pop_farthest ();

  return true;
 }

 return false;
}
//}}}
// pop_farthest {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void SETcache::pop_farthest () {
 //! In case we exceed Delete the last page
 //! New policy, delete the darkest element :TRICKY:
 //! Complexity O(1)
 if ((int)cache->size () > this->_max) {
  set<diskPage>::iterator first = cache->begin (); //! 0(1)
  set<diskPage>::reverse_iterator last = cache->rbegin (); //! O(1)

  uint64_t lowest  = (*first).point;
  uint64_t highest = (*last).point;

  //! If the victim belongs to the boundary of the node
  if (boundary_low < lowest || highest < boundary_upp) {

   //! POP the leftend element
   if (((uint64_t)ema - lowest) > ((uint64_t)highest - ema)) {

    pthread_mutex_lock (&mutex_queue_low);
    queue_lower.push (*first);
    pthread_mutex_unlock (&mutex_queue_low);

    pthread_mutex_lock (&mutex_match);
    cache->erase (*first);
    cache_time->erase (*first);
    pthread_mutex_unlock (&mutex_match);

    //! Pop the rightend element
   } else if ((uint64_t)highest > ema) { 

    pthread_mutex_lock (&mutex_queue_upp);
    queue_upper.push (*last);
    pthread_mutex_unlock (&mutex_queue_upp);

    pthread_mutex_lock (&mutex_match);
    cache->erase (*last);
    cache_time->erase (*last);
    pthread_mutex_unlock (&mutex_match);
   }

   //! Otherwise pop the oldest element :LRU:
 // } else {
 //  set<diskPage>::iterator oldest = cache_time->begin();
 //  uint64_t oldest_time = (*oldest).time;
 //  uint64_t oldest_item = (*oldest).point;

 //  //! Depends of the position
 //  if (oldest_item < ema) {
 //   pthread_mutex_lock (&mutex_queue_low);
 //   queue_lower.push (*oldest);
 //   pthread_mutex_unlock (&mutex_queue_low);

 //  } else {
 //   pthread_mutex_lock (&mutex_queue_upp);
 //   queue_upper.push (*oldest);
 //   pthread_mutex_unlock (&mutex_queue_upp);

 //  }
 //  pthread_mutex_lock (&mutex_match);

 //  cache_time->erase (oldest_time);
 //  cache->erase (oldest_item);

 //  pthread_mutex_unlock (&mutex_match);
 // }
  }
 }
}
// }}}
// update {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void SETcache::update (double low, double upp) {
 set<diskPage>::iterator low_i;
 set<diskPage>::iterator upp_i;
 set<diskPage>::iterator it;

 //! Set the iterators in the boundaries [ O(logn) ]
 pthread_mutex_lock (&mutex_match);
 low_i = cache->lower_bound (diskPage ((uint64_t)(low + .5)));
 pthread_mutex_unlock (&mutex_match);

 if (! (low_i == cache->end ()) && !(low_i == cache->begin ())) {

  //! Fill lower queue [ O(m1) ]
  pthread_mutex_lock (&mutex_queue_low);

  for (it = cache->begin (); it != low_i; it++) {
   queue_lower.push (*it);
  }

  pthread_mutex_unlock (&mutex_queue_low);

  pthread_mutex_lock (&mutex_match);
  cache->erase (cache->begin (), low_i);
  pthread_mutex_unlock (&mutex_match);
 }  

 pthread_mutex_lock (&mutex_match);
 upp_i = cache->upper_bound (diskPage ((uint64_t)(upp + .5)));
 pthread_mutex_unlock (&mutex_match);

 if (!(upp_i == cache->end ()) && !(upp_i == cache->begin ()))  {

  //! Fill upper queue [ O(m2) ]
  pthread_mutex_lock (&mutex_queue_upp);
  for (it = upp_i; it != cache->end (); it++)
   queue_upper.push (*it);

  pthread_mutex_unlock (&mutex_queue_upp);

  //! Delete those elements [ O(m1 + m2) ]
  pthread_mutex_lock (&mutex_match);
  cache->erase (upp_i, cache->end());
  pthread_mutex_unlock (&mutex_match);
 }
}

diskPage SETcache::get_diskPage (uint64_t idx) {
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
// insert {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool SETcache::insert (diskPage& dp) {
 //diskPage dp (idx);  
 diskPage key (idx);  

 auto it = cache->find (dp);
 bool found_same = (it != cache->end());
 
 if (count < _max) {
  if (found_same) {
   pthread_mutex_lock (&mutex_match);
   std::move (it, it, cache_time->begin());
   pthread_mutex_unlock (&mutex_match);

  } else {
   pthread_mutex_lock (&mutex_match);
   cache->insert (dp);
   cache_time->insert (dp);
   pthread_mutex_unlock (&mutex_match);
  }
 //! If the cache is full
 } else {  
  if (found_same) {
   pthread_mutex_lock (&mutex_match);
   cache_time->erase (key);
   cache_time->erase (dp);
   pthread_mutex_unlock (&mutex_match);
   // No need to update cache
   
  } else {
   pthread_mutex_lock (&mutex_match);
   cache->insert (dp);
   cache_time->insert (dp);
   pthread_mutex_unlock (&mutex_match);
   pop_farthest ();
  }
 } 
 return true;
}
//}}}
// lookup {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
diskPage SETcache::lookup (uint64_t idx) throw (std::out_of_range) {
 diskPage a (idx);
 auto victim = cache->find (a);
 if (victim != cache->end ())  //! If it is found O(log n)
  return *victim;

 else 
  throw std::out_of_range();
}
//}}}
