#include <cache.hh>

//char* lookup (int key); :TODO:

bool Cache::lookup (int key, char* output, size_t* s) {
 try {
  Chunk* out = this->_map [key]; 

 } catch (std::out_of_range& e) {
  return false;
 }

 memcpy (output, out->str, out->size);
 *s = out->second;

 return false;
}

bool Cache::insert (int key, char* output, size_t s) {
 char * tmp = new char (s);
 memcpy (tmp, output, s);
 this->_map [key] = PAIR (output, s);

 return true;
}

void Cache::discard () {
 if        (policies & CACHE_LRU) {
  discard_lru ();

 } else if (policies & CACHE_SPATIAL) {
  discard_spatial ();
 }
}
