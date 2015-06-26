#include "settings.hh"
#include <iostream>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <boost/foreach.hpp>
#include <ifaddrs.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define FINAL_PATH "/eclipse.json"

using std::cout;
using std::endl;
using std::vector;

// get_project_path {{{
bool Settings::get_project_path () 
{
  string home_location   = string(getenv("HOME")) + "/.eclipse.json";
  string system_location = "/etc/eclipse.json";

  if (hardcoded_path) {
    config_path = given_path;                                           // Finally the one of the constructor

  } else if (access(home_location.c_str(), F_OK) == EXIT_SUCCESS) {     // First at home
    config_path = home_location;

  } else if (access(system_location.c_str(), F_OK) == EXIT_SUCCESS) {   // Then at /etc
    config_path = system_location;

  } else {                               
#ifdef ECLIPSE_CONF_PATH
    config_path = string(ECLIPSE_CONF_PATH) + FINAL_PATH;               // Then configure one
#else
    return false;
#endif
  }

  return true;
}
// }}}
// load_settings {{{
//
bool Settings::load_settings ()
{
  get_project_path();
  boost::property_tree::json_parser::read_json (config_path, pt);

  return true;
}
// }}}
// getip {{{
string Settings::getip () const
{
  char if_ip [INET_ADDRSTRLEN];
  struct ifaddrs *ifAddrStruct = NULL, *ifa = NULL;
  string interface = pt.get<string> ("network.iface");

  getifaddrs (&ifAddrStruct);

  for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_name == interface)
      inet_ntop (AF_INET, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr, if_ip, INET_ADDRSTRLEN);
  }

  if(ifAddrStruct != NULL) freeifaddrs (ifAddrStruct);

  return string (if_ip);
}
// }}}
//Getters {{{
int Settings::port_mr ()           { return pt.get<int> ("network.port_mapreduce"); }
int Settings::port_cache ()        { return pt.get<int> ("network.port_cache"); }
int Settings::max_job ()           { return pt.get<int> ("max_job"); }
string Settings::operator[] (string str) { return pt.get<string> (str.c_str()); }
// }}}
// nodelist {{{
vector<string> Settings::nodelist () 
{
  vector<string> output;
  BOOST_FOREACH(ptree::value_type& v, pt.get_child ("network.nodes")) 
  {
    output.push_back (v.second.data());
  }
  return output;
}
