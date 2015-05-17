#include <settings.hh>

#include <iostream>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <boost/foreach.hpp>

#define FINAL_PATH "/eclipse.json"

using std::cout;
using std::endl;

bool Settings::get_project_path () 
{
  string home_location = string(getenv("HOME")) + "/.eclipse.json";
  if (access(home_location.c_str(), F_OK ) != -1) {
    project_path = string(ECLIPSE_CONF_PATH); 
    config_path = home_location;

  } else {
    if (hardcoded_path == true)
      project_path = string(ECLIPSE_CONF_PATH); 
    else 
      project_path = given_path;

    config_path = project_path + FINAL_PATH;
  }
 
  return true;
}

bool Settings::load_settings ()
{
  get_project_path();

  try 
  {
    boost::property_tree::json_parser::read_json (config_path, pt);
    pt.get<int> ("port");
    pt.get<int> ("dhtport");
    pt.get<int> ("max_job");
  } 
  catch (ptree_error& e) 
  {
    cout << e.what() << endl;
  }

  return true;
}

//Getters
const string Settings::lib_path () { return project_path; }
int Settings::port ()              { return pt.get<int> ("port"); }
int Settings::dhtport ()           { return pt.get<int> ("dhtport"); }
int Settings::max_job ()           { return pt.get<int> ("max_job"); }
string Settings::master_addr ()    { return pt.get<string> ("master_address"); }
string Settings::ipc_path ()    { return pt.get<string> ("IPC_path"); }

vector<string> Settings::nodelist () 
{
  vector<string> output;
  BOOST_FOREACH(ptree::value_type& v, pt.get_child ("nodes")) 
  {
    output.push_back (v.second.data());
  }
  return output;
}
