#ifndef __SETTINGS_HH_
#define __SETTINGS_HH_

#include <string>
#include <vector>

#include <boost/property_tree/ptree.hpp>

using std::string;
using std::vector;
using namespace boost::property_tree;

class Settings 
{
  protected:
    boost::property_tree::ptree pt;
    string project_path, config_path;

  public:
    Settings()  {}
    ~Settings() {}

    bool get_project_path ();
    bool load_settings ();

    //Getters
    const string lib_path ();
    int port ();
    int dhtport ();
    int max_job ();
    string master_addr ();
    vector<string> nodelist ();
};

#endif
