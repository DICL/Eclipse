#ifndef __SETTINGS_HH_
#define __SETTINGS_HH_

#include <string>
#include <vector>

#include <boost/property_tree/ptree.hpp>

using std::string;
using namespace boost::property_tree;

class Settings 
{
  protected:

    boost::property_tree::ptree pt;
    string config_path, given_path;
    bool hardcoded_path;
    bool get_project_path ();

  public:
    Settings() : hardcoded_path(false) { }
    Settings(string in) : given_path (in), hardcoded_path (true) { }
    ~Settings() {}

    bool load_settings ();

    //Getters
    string operator[] (string);
    string getip () const;
    int port_mr ();
    int port_cache ();
    int max_job ();
    std::vector<string> nodelist ();
};

#endif
