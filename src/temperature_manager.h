#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "app/app_state.h"

namespace temperature_manager {
void initialize();
void update();
void scanSensors();
void loadFromPreferences(Preferences& preferences, ConsoleProfile profile);
bool saveToPreferences(Preferences& preferences, ConsoleProfile profile);
void clearPersistedSettings(Preferences& preferences, ConsoleProfile profile);
String getDetectedSensorsJson();
}  // namespace temperature_manager
