#include "temperature_manager.h"

#include <DallasTemperature.h>
#include <OneWire.h>
#include <WString.h>

#include "config/pins.h"
#include "config/profiles.h"
#include "utils/logger.h"

DeviceAddress SONDE1_ID = {};
DeviceAddress SONDE2_ID = {};

namespace {
constexpr uint8_t kMaxSensors = 2;
constexpr uint32_t kSensorRefreshMs = 2000;
constexpr uint8_t kSensorNameMaxLength = 16;

OneWire oneWire(SONDES_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress discoveredAddresses[kMaxSensors];
char discoveredNames[kMaxSensors][kSensorNameMaxLength] = {"sonde1", "sonde2"};
float discoveredTemperatures[kMaxSensors] = {NAN, NAN};
uint8_t discoveredCount = 0;
uint32_t lastRefreshMs = 0;
bool initialized = false;

String deviceAddressToString(const DeviceAddress& address) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%02X%02X%02X%02X%02X%02X%02X%02X",
           address[0], address[1], address[2], address[3], address[4], address[5], address[6], address[7]);
  return String(buffer);
}

bool isAddressEmpty(const DeviceAddress& address) {
  for (uint8_t i = 0; i < 8; ++i) {
    if (address[i] != 0) {
      return false;
    }
  }
  return true;
}

void copyAddress(DeviceAddress& dst, const DeviceAddress& src) {
  for (uint8_t i = 0; i < 8; ++i) {
    dst[i] = src[i];
  }
}

void readTemperatures() {
  if (!initialized) {
    return;
  }

  logger::debugf("[TEMP] readTemperatures() - discoveredCount=%u", discoveredCount);
  
  auto& state = getAppState();
  sensors.requestTemperatures();
  delay(20);

  for (uint8_t i = 0; i < discoveredCount; ++i) {
    const float value = sensors.getTempC(discoveredAddresses[i]);
    if (value != DEVICE_DISCONNECTED_C) {
      discoveredTemperatures[i] = value;
      state.sensorConfig[i].currentTemperature = value;
      state.sensorConfig[i].detected = true;
      logger::tracef("[TEMP] sensor %u read successfully: %.1fC", i + 1, value);
    } else {
      discoveredTemperatures[i] = NAN;
      state.sensorConfig[i].currentTemperature = NAN;
      state.sensorConfig[i].detected = false;
      logger::warnf("[TEMP] sensor %u disconnected", i + 1);
    }
  }

  if (discoveredCount < 2) {
    state.sensorConfig[1].currentTemperature = NAN;
    state.sensorConfig[1].detected = false;
  }
}

void discoverSensors() {
  discoveredCount = 0;
  
  logger::debugf("[TEMP] discoverSensors() starting - SONDES_PIN=%u", SONDES_PIN);
  
  // When rescanning, reconfigure the GPIO and reinitialize the bus
  if (initialized) {
    logger::trace("[TEMP] rescanning: reconfiguring GPIO pin as INPUT_PULLUP");
    pinMode(SONDES_PIN, INPUT_PULLUP);
    delay(50);
    
    logger::trace("[TEMP] rescanning: calling sensors.begin() for re-initialization");
    sensors.begin();
    delay(100);
  }
  
  sensors.setWaitForConversion(false);
  sensors.setResolution(12);
  logger::trace("[TEMP] sensors configuration (waitForConversion=false, resolution=12) applied");

  const uint8_t sensorCount = sensors.getDeviceCount();
  logger::debugf("[TEMP] sensors.getDeviceCount() returned: %u", sensorCount);
  logger::infof("[TEMP] discovered %u devices on pin %u", sensorCount, SONDES_PIN);

  if (sensorCount == 0) {
    logger::warn("[TEMP] no DS18B20 device detected");
    logger::debugf("[TEMP] GPIO level (1=HIGH/idle, 0=LOW): %d", digitalRead(SONDES_PIN));
    return;
  }

  for (uint8_t i = 0; i < min(sensorCount, kMaxSensors); ++i) {
    logger::debugf("[TEMP] attempting to read address for sensor index %u", i);
    
    if (!sensors.getAddress(discoveredAddresses[i], i)) {
      logger::warnf("[TEMP] failed to read address for sensor %u", i + 1);
      continue;
    }

    if (isAddressEmpty(discoveredAddresses[i])) {
      logger::warnf("[TEMP] empty address for sensor %u", i + 1);
      continue;
    }

    discoveredCount = i + 1;
    logger::debugf("[TEMP] sensor %u successfully read with address=%s", i + 1, deviceAddressToString(discoveredAddresses[i]).c_str());
    logger::infof("[TEMP] sensor %u address=%s", i + 1, deviceAddressToString(discoveredAddresses[i]).c_str());
  }

  for (uint8_t i = 0; i < discoveredCount; ++i) {
    discoveredTemperatures[i] = NAN;
  }

  auto& state = getAppState();
  state.sensorConfig[0].detected = discoveredCount > 0;
  state.sensorConfig[1].detected = discoveredCount > 1;
  state.sensorConfig[0].currentTemperature = NAN;
  state.sensorConfig[1].currentTemperature = NAN;
}

void applySensorNames(const char* name1, const char* name2) {
  if (name1 != nullptr && strlen(name1) > 0) {
    strncpy(discoveredNames[0], name1, kSensorNameMaxLength - 1);
    discoveredNames[0][kSensorNameMaxLength - 1] = '\0';
  } else {
    strncpy(discoveredNames[0], "sonde1", kSensorNameMaxLength - 1);
    discoveredNames[0][kSensorNameMaxLength - 1] = '\0';
  }

  if (name2 != nullptr && strlen(name2) > 0) {
    strncpy(discoveredNames[1], name2, kSensorNameMaxLength - 1);
    discoveredNames[1][kSensorNameMaxLength - 1] = '\0';
  } else {
    strncpy(discoveredNames[1], "sonde2", kSensorNameMaxLength - 1);
    discoveredNames[1][kSensorNameMaxLength - 1] = '\0';
  }
}

void loadPersistedSensorSettings(Preferences& preferences, ConsoleProfile profile) {
  const char* prefix = profile == ConsoleProfile::PS4_PRO ? "p4" : (profile == ConsoleProfile::PS3_FAT ? "p3" : "p5");
  char key1[16];
  char key2[16];
  char keyName1[24];
  char keyName2[24];
  char keyIdle1[24];
  char keyMax1[24];
  char keyIdle2[24];
  char keyMax2[24];
  snprintf(key1, sizeof(key1), "%s_s1_addr", prefix);
  snprintf(key2, sizeof(key2), "%s_s2_addr", prefix);
  snprintf(keyName1, sizeof(keyName1), "%s_s1_name", prefix);
  snprintf(keyName2, sizeof(keyName2), "%s_s2_name", prefix);
  snprintf(keyIdle1, sizeof(keyIdle1), "%s_s1_t_idle", prefix);
  snprintf(keyMax1, sizeof(keyMax1), "%s_s1_t_max", prefix);
  snprintf(keyIdle2, sizeof(keyIdle2), "%s_s2_t_idle", prefix);
  snprintf(keyMax2, sizeof(keyMax2), "%s_s2_t_max", prefix);

  auto& state = getAppState();
  const uint8_t profileIndex = static_cast<uint8_t>(profile);
  const float defaultIdle = state.profileThresholds[profileIndex].temperatureIdle;
  const float defaultMax = state.profileThresholds[profileIndex].temperatureMax;
  String addr1 = preferences.getString(key1, "");
  String addr2 = preferences.getString(key2, "");
  String name1 = preferences.getString(keyName1, "sonde1");
  String name2 = preferences.getString(keyName2, "sonde2");

  if (addr1.length() > 0) {
    state.sensorConfig[0].address = addr1;
    state.sensorConfig[0].name = name1;
    state.sensorConfig[0].enabled = true;
  } else {
    state.sensorConfig[0].address = "";
    state.sensorConfig[0].name = "sonde1";
    state.sensorConfig[0].enabled = false;
  }
  state.sensorConfig[0].temperatureIdleThreshold = preferences.getFloat(keyIdle1, defaultIdle);
  state.sensorConfig[0].temperatureMaxThreshold = preferences.getFloat(keyMax1, defaultMax);

  if (addr2.length() > 0) {
    state.sensorConfig[1].address = addr2;
    state.sensorConfig[1].name = name2;
    state.sensorConfig[1].enabled = true;
  } else {
    state.sensorConfig[1].address = "";
    state.sensorConfig[1].name = "sonde2";
    state.sensorConfig[1].enabled = false;
  }
  state.sensorConfig[1].temperatureIdleThreshold = preferences.getFloat(keyIdle2, defaultIdle);
  state.sensorConfig[1].temperatureMaxThreshold = preferences.getFloat(keyMax2, defaultMax);
}

void setCurrentProfileSensorConfig(ConsoleProfile profile) {
  auto& state = getAppState();
  const uint8_t index = static_cast<uint8_t>(profile);
  const auto& thresholds = state.profileThresholds[index];

  state.sensorConfig[0].temperatureIdleThreshold = thresholds.temperatureIdle;
  state.sensorConfig[0].temperatureMaxThreshold = thresholds.temperatureMax;
  state.sensorConfig[1].temperatureIdleThreshold = thresholds.temperatureIdle;
  state.sensorConfig[1].temperatureMaxThreshold = thresholds.temperatureMax;
}
}  // namespace

void temperature_manager::initialize() {
  logger::debugf("[TEMP] initialize() starting - setting up OneWire on pin %u", SONDES_PIN);
  
  pinMode(SONDES_PIN, INPUT_PULLUP);
  logger::trace("[TEMP] pinMode(SONDES_PIN, INPUT_PULLUP) configured");
  
  // Initial sensors setup
  logger::trace("[TEMP] initial sensors.begin() call");
  sensors.begin();
  sensors.setWaitForConversion(false);
  sensors.setResolution(12);
  
  delay(100);
  logger::trace("[TEMP] delay(100ms) after initial setup");
  
  discoveredCount = 0;
  initialized = true;
  logger::info("[TEMP] DS18B20 manager initialized");
  
  // Now perform the discovery scan
  discoverSensors();
  lastRefreshMs = millis();
}

void temperature_manager::scanSensors() {
  if (!initialized) {
    logger::warn("[TEMP] scanSensors() called but not initialized, calling initialize()");
    initialize();
    return;
  }
  logger::info("[TEMP] rescanning DS18B20 sensors");
  logger::debugf("[TEMP] Current discoveredCount before rescan: %u", discoveredCount);
  discoverSensors();
  logger::debugf("[TEMP] Current discoveredCount after rescan: %u", discoveredCount);
}

void temperature_manager::update() {
  if (!initialized) {
    return;
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastRefreshMs < kSensorRefreshMs) {
    return;
  }
  lastRefreshMs = nowMs;

  readTemperatures();
  logger::trace("[TEMP] temperature refresh completed");
}

void temperature_manager::loadFromPreferences(Preferences& preferences, ConsoleProfile profile) {
  loadPersistedSensorSettings(preferences, profile);
  setCurrentProfileSensorConfig(profile);
}

bool temperature_manager::saveToPreferences(Preferences& preferences, ConsoleProfile profile) {
  auto& state = getAppState();
  const char* prefix = profile == ConsoleProfile::PS4_PRO ? "p4" : (profile == ConsoleProfile::PS3_FAT ? "p3" : "p5");
  char key1[16];
  char key2[16];
  char keyName1[24];
  char keyName2[24];
  char keyIdle1[24];
  char keyMax1[24];
  char keyIdle2[24];
  char keyMax2[24];
  snprintf(key1, sizeof(key1), "%s_s1_addr", prefix);
  snprintf(key2, sizeof(key2), "%s_s2_addr", prefix);
  snprintf(keyName1, sizeof(keyName1), "%s_s1_name", prefix);
  snprintf(keyName2, sizeof(keyName2), "%s_s2_name", prefix);
  snprintf(keyIdle1, sizeof(keyIdle1), "%s_s1_t_idle", prefix);
  snprintf(keyMax1, sizeof(keyMax1), "%s_s1_t_max", prefix);
  snprintf(keyIdle2, sizeof(keyIdle2), "%s_s2_t_idle", prefix);
  snprintf(keyMax2, sizeof(keyMax2), "%s_s2_t_max", prefix);

  const size_t written1 = preferences.putString(key1, state.sensorConfig[0].address);
  const size_t written2 = preferences.putString(key2, state.sensorConfig[1].address);
  const size_t written3 = preferences.putString(keyName1, state.sensorConfig[0].name);
  const size_t written4 = preferences.putString(keyName2, state.sensorConfig[1].name);
  const bool ok1 = written1 > 0;
  const bool ok2 = written2 > 0;
  const bool ok3 = written3 > 0;
  const bool ok4 = written4 > 0;

  if (!ok1 || !ok2 || !ok3 || !ok4) {
    logger::error("[TEMP] failed to persist temperature sensor settings");
    return false;
  }

  preferences.putFloat(keyIdle1, state.sensorConfig[0].temperatureIdleThreshold);
  preferences.putFloat(keyMax1, state.sensorConfig[0].temperatureMaxThreshold);
  preferences.putFloat(keyIdle2, state.sensorConfig[1].temperatureIdleThreshold);
  preferences.putFloat(keyMax2, state.sensorConfig[1].temperatureMaxThreshold);
  preferences.putFloat("temperature_idle", state.profileThresholds[static_cast<uint8_t>(profile)].temperatureIdle);
  preferences.putFloat("temperature_max", state.profileThresholds[static_cast<uint8_t>(profile)].temperatureMax);
  logger::info("[TEMP] temperature sensor settings saved");
  return true;
}

void temperature_manager::clearPersistedSettings(Preferences& preferences, ConsoleProfile profile) {
  const char* prefix = profile == ConsoleProfile::PS4_PRO ? "p4" : (profile == ConsoleProfile::PS3_FAT ? "p3" : "p5");
  char key1[16];
  char key2[16];
  char keyIdle1[24];
  char keyMax1[24];
  char keyIdle2[24];
  char keyMax2[24];
  snprintf(key1, sizeof(key1), "%s_s1_addr", prefix);
  snprintf(key2, sizeof(key2), "%s_s2_addr", prefix);
  snprintf(keyIdle1, sizeof(keyIdle1), "%s_s1_t_idle", prefix);
  snprintf(keyMax1, sizeof(keyMax1), "%s_s1_t_max", prefix);
  snprintf(keyIdle2, sizeof(keyIdle2), "%s_s2_t_idle", prefix);
  snprintf(keyMax2, sizeof(keyMax2), "%s_s2_t_max", prefix);
  preferences.remove(key1);
  preferences.remove(key2);
  preferences.remove(keyIdle1);
  preferences.remove(keyMax1);
  preferences.remove(keyIdle2);
  preferences.remove(keyMax2);
  preferences.remove("temperature_idle");
  preferences.remove("temperature_max");
}

String temperature_manager::getDetectedSensorsJson() {
  String json = "{";
  for (uint8_t i = 0; i < discoveredCount; ++i) {
    if (i > 0) {
      json += ",";
    }
    json += String("\"") + String(i) + String("\":{\"address\":\"") + deviceAddressToString(discoveredAddresses[i]) + String("\",\"name\":\"") + discoveredNames[i] + String("\",\"temperature\":") + (isnan(discoveredTemperatures[i]) ? String("null") : String(discoveredTemperatures[i], 1)) + String("}");
  }
  json += "}";
  return json;
}
