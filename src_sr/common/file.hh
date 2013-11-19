#ifndef __FILE_H__
#define __FILE_H__
//
//
class File {
 
 public:
  File (const char * _name) {
   strncpy (filename, _name, 32);
  } 

  virtual ~File () {

  }

  void serialize (); 
  size_t get_size () { return size; }
  const char* get_filename () { return filename; }


 private:
  char filename [32];
  uint8_t* data;
  size_t size; 
};
//
//
#endif 
