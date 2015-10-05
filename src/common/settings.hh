/* \file       settings.hh
 * @author     Vicente Adolfo Bolea Sanchez
 * @brief      This is the implementation file of Settings 
 * 
 * @section 1 Configuration file path
 * Settings will read the configuration file eclipse.json and
 * load all the necessary properties. The path of the eclipse.json
 * will be:
 *  -# ~/.eclipse.json
 *  -# /etc/eclipse.json
 *  -# Constructor path @see Settings::Settings(std::string)
 *  -# Hardcoded path, setted using autoconf
 *
 * @section 2 Usage
 * The way it was designed to be used was:
 *
 * @code
 * Settings setted = Settings().load();
 * string path1 = setted.get<string>("path1");
 * @endcode
 *
 * @attention This class uses the P.I.M.P.L. (Pointer to implementation) idiom
 *            this reduces the complexity of the interface and compilation time. 
 */
#ifndef __SETTINGS_HH_
#define __SETTINGS_HH_

#include <string>
#include <memory>

class Settings 
{
  private:
    class SettingsImpl;
    std::unique_ptr<SettingsImpl> impl;

  public:
    Settings();
    Settings(std::string);

    Settings(Settings&&);          //! Move operators
    void operator=(Settings&&);    //!

    ~Settings();

    Settings& load () &;
    Settings&& load () &&;

    template <typename T> T get (std::string) const;
    std::string getip () const;
};

#endif
