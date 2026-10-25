/*
 * settings-amiga.cc - Settings::Settings/load/save without SDL
 * (replaces src/settings.cc): the settings live next to the program.
 */

#include "src/settings.h"

#include <fstream>
#include <string>

#include "src/log.h"

Settings::Settings() {
  path = "PROGDIR:Settlers.ini";
}

Settings &
Settings::get_instance() {
  static Settings instance;
  return instance;
}

bool
Settings::load() {
  std::ifstream test(path.c_str());
  if (!test.is_open()) {
    return false;
  }
  test.close();
  if (!config.load(path)) {
    Log::Warn["settings"] << "Unable to read the settings from " << path;
    config = ConfigFile();
    return false;
  }
  Log::Info["settings"] << "Settings loaded from " << path;
  return true;
}

bool
Settings::save() {
  if (path.empty() || !config.save(path)) {
    Log::Warn["settings"] << "Unable to save the settings to " << path;
    return false;
  }
  return true;
}
