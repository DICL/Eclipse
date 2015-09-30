#include "cache_slave.hh"
#include <iostream>
#include <ifstream>
#include <ofstream>
#include <streambuf>

using namespace std;

// async_file_send {{{
void Cache_slave::async_file_send  (std::string str, Node& node) {
  string& value = cache.lookup(str);
  
  node->send(value);
}
// }}}
// async_data_send {{{
void Cache_slave::async_data_send  (std::string str, Node& node) {
  string& value = cache.lookup(str);
  
  node->send(value); 
}
// }}}
// async_file_read {{{
void Cache_slave::async_file_read  (std::string str, Node& node) {
  ifstream file (str, ifstream::in);
  string content (istreambuf_iterator<char>(file), istreambuf_iterator<char>());

  thecache[str] = content;
  Node->send(content);
}
// }}}
// async_file_write {{{
void Cache_slave::async_file_write (std::string filename, Node& node) {
  ofstream file (filename, ofstream::out);
  out << data;
  out.close();
}
// }}}
// async_request_file_read {{{
void Cache_slave::async_request_file_read  (std::string str, Node& node) {
  string msg = "RemoteFileRead ";
  msg += str;
  Node->send(msg);
}
// }}}
// async_request_file_write {{{
void Cache_slave::async_request_file_write (std::string str, Node& node) {

}
// }}}
// async_request_data_read {{{
void Cache_slave::async_request_data_read  (std::string str, Node& node) {

}
// }}}
// async_request_data_write {{{
void Cache_slave::async_request_data_write (std::string str, Node& node) {

}
// }}}
