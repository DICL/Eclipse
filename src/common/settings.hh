#ifndef __SETTINGS_HH_
#define __SETTINGS_HH_

#include <string>
#include <map>
#include <vector>
#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <boost/foreach.hpp>

#define FINAL_PATH "/etc/eclipse.json"

using std::string;
using std::map;
using std::vector;
using std::cout;
using std::endl;
using namespace boost::property_tree;

class Settings 
{
  protected:
    boost::property_tree::ptree pt;
    string project_path, config_path;

  public:
    Settings()  {}
    ~Settings() {}

    bool get_project_path () 
    {
      project_path = getenv ("ECLIPSE_PATH");
      config_path = project_path + FINAL_PATH;
      return true;
    }

    bool load_settings ()
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
    const string lib_path () { return project_path; }
    int port ()        { return pt.get<int> ("port"); }
    int dhtport ()     { return pt.get<int> ("dhtport"); }
    int max_job ()    { return pt.get<int> ("max_job"); }
    string master_addr ()    { return pt.get<string> ("master_address"); }
    //#vector NODELIST    { return 
};

#endif
