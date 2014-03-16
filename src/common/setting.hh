#ifndef SETTING_59U7M36G

#define SETTING_59U7M36G

#define TYPE_INT   0x0
#define TYPE_CHAR  0x1

class settings_value_t {
 public:
  settings_value_t (uint64_t);
  settings_value_t (const char*);
  int get_type ();
  uint64_t operator() get_value ();
  char* operator() get_value ();

 private:
  int type;
  union {
   uint64_t type_int;
   char*    type_str;
  };  
};


class settings_t {
 public:
  settigns_t ();
  settigns_t (const settings_t&);
  settings_t& operator= (const settings_t&);
  ~settigns_t ();

  template <class TYPE>
  void set (int, TYPE);
  auto set (int) -> dectype (TYPE);

 private:
  std::unordered_map<int, settings_key_t> _map;
};

#endif /* end of include guard: SETTING_59U7M36G */
