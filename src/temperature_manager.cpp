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
// Conversion DS18B20 en 12 bits : 750 ms max.
constexpr uint32_t kConversionWaitMs = 800;
constexpr uint8_t kAddressHexLength = 16;

OneWire oneWire(SONDES_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress discoveredAddresses[kMaxSensors];
float discoveredTemperatures[kMaxSensors] = {NAN, NAN};
uint8_t discoveredCount = 0;
uint32_t lastRequestMs = 0;
bool conversionPending = false;
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

int hexDigitValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool parseAddress(const String& hex, DeviceAddress& out) {
  if (hex.length() != kAddressHexLength) {
    return false;
  }
  for (uint8_t i = 0; i < 8; ++i) {
    const int high = hexDigitValue(hex[i * 2]);
    const int low = hexDigitValue(hex[i * 2 + 1]);
    if (high < 0 || low < 0) {
      return false;
    }
    out[i] = static_cast<uint8_t>((high << 4) | low);
  }
  return true;
}

const char* profilePrefix(ConsoleProfile profile) {
  return getConsoleProfileStoragePrefix(profile);
}

// Cles NVS limitees a 15 caracteres : "<prefixe>_<suffixe>".
void makeKey(char (&buffer)[24], const char* prefix, const char* suffix) {
  snprintf(buffer, sizeof(buffer), "%s_%s", prefix, suffix);
}

void makeSlotPrefix(char (&buffer)[8], const char* prefix, uint8_t slot) {
  snprintf(buffer, sizeof(buffer), "%s_s%u", prefix, static_cast<unsigned>(slot + 1));
}

// Adresse effective d'un emplacement : adresse choisie par l'utilisateur ou,
// si aucune sonde n'est configuree, ordre de decouverte du bus.
bool resolveSlotAddress(uint8_t slot, DeviceAddress& out) {
  auto& state = getAppState();
  const SensorConfig& config = state.sensorConfig[slot];
  if (config.enabled && config.address.length() > 0) {
    return parseAddress(config.address, out);
  }

  if (state.sensorConfig[0].enabled || state.sensorConfig[1].enabled) {
    return false;
  }
  if (slot < discoveredCount) {
    copyAddress(out, discoveredAddresses[slot]);
    return true;
  }
  return false;
}

void updateDetectionFlags() {
  auto& state = getAppState();
  for (uint8_t slot = 0; slot < kMaxSensors; ++slot) {
    DeviceAddress address;
    state.sensorConfig[slot].detected = resolveSlotAddress(slot, address) && sensors.isConnected(address);
    state.sensorConfig[slot].currentTemperature = NAN;
  }
}

void readTemperatures() {
  auto& state = getAppState();
  for (uint8_t slot = 0; slot < kMaxSensors; ++slot) {
    DeviceAddress address;
    const bool wasDetected = state.sensorConfig[slot].detected;
    float value = DEVICE_DISCONNECTED_C;
    if (resolveSlotAddress(slot, address)) {
      value = sensors.getTempC(address);
    }

    const bool ok = value != DEVICE_DISCONNECTED_C;
    state.sensorConfig[slot].detected = ok;
    state.sensorConfig[slot].currentTemperature = ok ? value : NAN;

    // Journalise uniquement les changements d'etat pour ne pas polluer la console.
    if (ok && !wasDetected) {
      logger::infof("[TEMP] sensor %u connected", static_cast<unsigned>(slot + 1));
    } else if (!ok && wasDetected) {
      logger::warnf("[TEMP] sensor %u disconnected", static_cast<unsigned>(slot + 1));
    }
  }
}

// Diagnostic bas niveau du bus 1-Wire (logs DEBUG) pour localiser une panne de cablage.
void diagnoseBus() {
  pinMode(SONDES_PIN, INPUT);
  delay(2);
  const int levelNoPullup = digitalRead(SONDES_PIN);
  pinMode(SONDES_PIN, INPUT_PULLUP);
  delay(2);
  const int levelInternalPullup = digitalRead(SONDES_PIN);
  logger::debugf("[TEMP][DIAG] idle level: no pull-up=%d, internal pull-up=%d (attendu: 1 et 1)",
                 levelNoPullup, levelInternalPullup);
  if (levelInternalPullup == 0) {
    logger::warn("[TEMP][DIAG] DATA bloque a 0 : court-circuit vers GND, ou sonde alimentee a l'envers (GND/3V inverses)");
  } else if (levelNoPullup == 0) {
    logger::warn("[TEMP][DIAG] pas de pull-up externe : la resistance n'est pas reliee entre 3V et DATA");
  }

  // Temps de remontee du bus apres un niveau bas : long = pas de pull-up externe.
  pinMode(SONDES_PIN, OUTPUT);
  digitalWrite(SONDES_PIN, LOW);
  delayMicroseconds(200);
  noInterrupts();
  pinMode(SONDES_PIN, INPUT);
  const uint32_t startUs = micros();
  while (digitalRead(SONDES_PIN) == 0 && micros() - startUs < 2000) {
  }
  const uint32_t riseUs = micros() - startUs;
  interrupts();
  logger::debugf("[TEMP][DIAG] rise time after LOW: %lu us (4.7k attendu < 20 us, >= 2000 = pas de pull-up)",
                 static_cast<unsigned long>(riseUs));

  pinMode(SONDES_PIN, INPUT_PULLUP);
  for (uint8_t attempt = 1; attempt <= 3; ++attempt) {
    const uint8_t presence = oneWire.reset();
    logger::debugf("[TEMP][DIAG] reset attempt %u -> presence pulse=%u", attempt, presence);
    delay(5);
  }

  oneWire.reset_search();
  uint8_t rom[8];
  uint8_t found = 0;
  while (oneWire.search(rom) && found < 8) {
    const bool crcOk = OneWire::crc8(rom, 7) == rom[7];
    logger::debugf("[TEMP][DIAG] raw ROM %u: %02X%02X%02X%02X%02X%02X%02X%02X family=0x%02X crc=%s", found + 1,
                   rom[0], rom[1], rom[2], rom[3], rom[4], rom[5], rom[6], rom[7], rom[0], crcOk ? "OK" : "KO");
    ++found;
  }
  oneWire.reset_search();
  logger::debugf("[TEMP][DIAG] raw search found %u device(s)", found);
}

void discoverSensors() {
  discoveredCount = 0;
  logger::debugf("[TEMP] discoverSensors() on pin %u", SONDES_PIN);

  diagnoseBus();

  pinMode(SONDES_PIN, INPUT_PULLUP);
  sensors.begin();
  sensors.setWaitForConversion(false);
  sensors.setResolution(12);

  const uint8_t sensorCount = sensors.getDeviceCount();
  logger::infof("[TEMP] discovered %u devices on pin %u", sensorCount, SONDES_PIN);

  if (sensorCount == 0) {
    logger::warn("[TEMP] no DS18B20 device detected");
    logger::debugf("[TEMP] GPIO level (1=HIGH/idle, 0=LOW): %d", digitalRead(SONDES_PIN));
  }

  for (uint8_t i = 0; i < sensorCount && discoveredCount < kMaxSensors; ++i) {
    DeviceAddress address;
    if (!sensors.getAddress(address, i) || isAddressEmpty(address)) {
      logger::warnf("[TEMP] invalid address for device index %u", i);
      continue;
    }

    copyAddress(discoveredAddresses[discoveredCount], address);
    discoveredTemperatures[discoveredCount] = NAN;
    logger::infof("[TEMP] sensor %u address=%s", static_cast<unsigned>(discoveredCount + 1), deviceAddressToString(address).c_str());
    ++discoveredCount;
  }

  conversionPending = false;
  updateDetectionFlags();
}

// Lecture bloquante (~750 ms) pour afficher une temperature a cote de chaque adresse detectee.
void readDiscoveredTemperatures() {
  if (discoveredCount == 0) {
    return;
  }

  sensors.setWaitForConversion(true);
  sensors.requestTemperatures();
  sensors.setWaitForConversion(false);

  for (uint8_t i = 0; i < discoveredCount; ++i) {
    const float value = sensors.getTempC(discoveredAddresses[i]);
    discoveredTemperatures[i] = value != DEVICE_DISCONNECTED_C ? value : NAN;
    logger::debugf("[TEMP] sensor %u raw temperature=%.2f", static_cast<unsigned>(i + 1), value);
  }
}

void loadPersistedSensorSettings(Preferences& preferences, ConsoleProfile profile) {
  const char* prefix = profilePrefix(profile);
  auto& state = getAppState();
  const ProfileThresholds& profileThresholds = state.profileThresholds[static_cast<uint8_t>(profile)];

  for (uint8_t slot = 0; slot < kMaxSensors; ++slot) {
    char slotPrefix[8];
    makeSlotPrefix(slotPrefix, prefix, slot);

    char key[24];
    SensorConfig& config = state.sensorConfig[slot];
    const String defaultName = String("sonde") + String(slot + 1);

    makeKey(key, slotPrefix, "addr");
    config.address = preferences.getString(key, "");
    config.enabled = config.address.length() > 0;

    makeKey(key, slotPrefix, "name");
    config.name = config.enabled ? preferences.getString(key, defaultName) : defaultName;
    if (config.name.length() == 0) {
      config.name = defaultName;
    }

    makeKey(key, slotPrefix, "t_idle");
    config.temperatureIdleThreshold = preferences.getFloat(key, profileThresholds.temperatureIdle);
    makeKey(key, slotPrefix, "t_max");
    config.temperatureMaxThreshold = preferences.getFloat(key, profileThresholds.temperatureMax);
  }
}
}  // namespace

void temperature_manager::initialize() {
  logger::debugf("[TEMP] initialize() - OneWire on pin %u", SONDES_PIN);

  initialized = true;
  discoverSensors();
  // Premiere requete de conversion des le premier update().
  lastRequestMs = millis() - kSensorRefreshMs;
  logger::info("[TEMP] DS18B20 manager initialized");
}

void temperature_manager::scanSensors() {
  if (!initialized) {
    initialize();
    return;
  }

  logger::info("[TEMP] rescanning DS18B20 sensors");
  discoverSensors();
  readDiscoveredTemperatures();
}

void temperature_manager::update() {
  if (!initialized) {
    return;
  }

  const uint32_t nowMs = millis();
  if (conversionPending) {
    if (nowMs - lastRequestMs >= kConversionWaitMs) {
      readTemperatures();
      conversionPending = false;
    }
    return;
  }

  if (nowMs - lastRequestMs >= kSensorRefreshMs) {
    sensors.requestTemperatures();
    lastRequestMs = nowMs;
    conversionPending = true;
  }
}

void temperature_manager::loadFromPreferences(Preferences& preferences, ConsoleProfile profile) {
  loadPersistedSensorSettings(preferences, profile);
}

bool temperature_manager::saveToPreferences(Preferences& preferences, ConsoleProfile profile) {
  auto& state = getAppState();
  const char* prefix = profilePrefix(profile);
  const ProfileThresholds& profileThresholds = state.profileThresholds[static_cast<uint8_t>(profile)];
  bool ok = true;

  auto saveString = [&](const char* keyPrefix, const char* suffix, const String& value) {
    char key[24];
    makeKey(key, keyPrefix, suffix);
    if (preferences.putString(key, value) != value.length()) {
      logger::errorf("[TEMP] failed to persist key=%s", key);
      ok = false;
    }
  };
  auto saveFloat = [&](const char* keyPrefix, const char* suffix, float value) {
    char key[24];
    makeKey(key, keyPrefix, suffix);
    if (preferences.putFloat(key, value) != sizeof(float)) {
      logger::errorf("[TEMP] failed to persist key=%s", key);
      ok = false;
    }
  };

  for (uint8_t slot = 0; slot < kMaxSensors; ++slot) {
    char slotPrefix[8];
    makeSlotPrefix(slotPrefix, prefix, slot);
    const SensorConfig& config = state.sensorConfig[slot];
    saveString(slotPrefix, "addr", config.address);
    saveString(slotPrefix, "name", config.name);
    saveFloat(slotPrefix, "t_idle", config.temperatureIdleThreshold);
    saveFloat(slotPrefix, "t_max", config.temperatureMaxThreshold);
  }

  saveFloat(prefix, "t_idle", profileThresholds.temperatureIdle);
  saveFloat(prefix, "t_max", profileThresholds.temperatureMax);

  if (ok) {
    logger::info("[TEMP] temperature sensor settings saved");
  }
  return ok;
}

void temperature_manager::clearPersistedSettings(Preferences& preferences, ConsoleProfile profile) {
  const char* prefix = profilePrefix(profile);
  const char* slotSuffixes[] = {"addr", "name", "t_idle", "t_max"};
  char key[24];

  for (uint8_t slot = 0; slot < kMaxSensors; ++slot) {
    char slotPrefix[8];
    makeSlotPrefix(slotPrefix, prefix, slot);
    for (const char* suffix : slotSuffixes) {
      makeKey(key, slotPrefix, suffix);
      preferences.remove(key);
    }
  }

  makeKey(key, prefix, "t_idle");
  preferences.remove(key);
  makeKey(key, prefix, "t_max");
  preferences.remove(key);
}

bool temperature_manager::isValidAddress(const String& address) {
  DeviceAddress parsed;
  return parseAddress(address, parsed);
}

String temperature_manager::getDetectedSensorsJson() {
  String json = "{";
  for (uint8_t i = 0; i < discoveredCount; ++i) {
    if (i > 0) {
      json += ",";
    }
    json += String("\"") + String(i) + String("\":{\"address\":\"") + deviceAddressToString(discoveredAddresses[i]) +
            String("\",\"temperature\":") +
            (isnan(discoveredTemperatures[i]) ? String("null") : String(discoveredTemperatures[i], 1)) + String("}");
  }
  json += "}";
  return json;
}
