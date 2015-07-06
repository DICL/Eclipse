#ifndef __SETTINGS_HH_
#define __SETTINGS_HH_

#include <string>

class Settings 
{
  private:
    class SettingsImpl;
    SettingsImpl* impl;

  public:
    Settings();
    Settings(std::string);
    ~Settings();

    bool load ();

    template <typename T> T get (std::string);
    std::string getip ();
};

#endif
