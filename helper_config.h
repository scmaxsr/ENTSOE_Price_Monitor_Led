#ifndef HELPER_CONFIG_H
#define HELPER_CONFIG_H

#include <FS.h>
#include "settings.h"

// Manage configuration stored in SPIFFS as JSON.
bool loadConfig();
bool saveConfig();
bool hasValidConfig();
void clearConfig();

// Global config object (defined in the main sketch)
extern Config config;

#endif
