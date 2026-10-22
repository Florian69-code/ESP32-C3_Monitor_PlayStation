#include "web_ui.h"

#include <ESP.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <stdlib.h>

#include "../app/app_state.h"
#include "../app/app_logic.h"
#include "../config/log_config.h"
#include "../config/profiles.h"
#include "../config/web_config.h"
#include "../session_stats.h"
#include "web_pages.h"
#include "web_storage.h"
#include "../temperature_manager.h"
#include "../utils/logger.h"

namespace {

bool gIsAuthenticated = false;

// Construit la valeur attendue du cookie de session authentifie.
String getAuthCookieValue() {
  return String(web_config::WEB_SESSION_COOKIE) + "=authorized";
}

// Redirection HTTP 303 avec cache desactive.
void redirectTo(WebServer& server, const char* url) {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Location", url);
  server.send(303, "text/plain", "");
}

// Rendu de la page console en utilisant l'etat courant applicatif.
String getConsolePage() {
  auto& state = getAppState();
  return web_pages::getConsolePage(
      "Console ESP32-C3",
      logger::getHtmlLogContent(),
      state.webLogRefreshSeconds,
      state.webLogLevel);
}

// Variante avec message de statut (erreur/succes) injecte dans la page.
String getConsolePage(const String& statusMessage) {
  auto& state = getAppState();
  return web_pages::getConsolePage(
      "Console ESP32-C3",
      logger::getHtmlLogContent(),
      state.webLogRefreshSeconds,
      state.webLogLevel,
      statusMessage);
}

// Parse strict d'un float (echoue si caracteres restants).
bool parseFloatArg(const String& rawValue, float& parsedValue) {
  if (rawValue.length() == 0) {
    return false;
  }

  char* endPointer = nullptr;
  parsedValue = strtof(rawValue.c_str(), &endPointer);
  return endPointer != rawValue.c_str() && *endPointer == '\0';
}

// Parse strict d'un entier non signe 8 bits.
bool parseUInt8Arg(const String& rawValue, uint8_t& parsedValue) {
  if (rawValue.length() == 0) {
    return false;
  }

  char* endPointer = nullptr;
  const long value = strtol(rawValue.c_str(), &endPointer, 10);
  if (endPointer == rawValue.c_str() || *endPointer != '\0') {
    return false;
  }
  if (value < 0 || value > 255) {
    return false;
  }

  parsedValue = static_cast<uint8_t>(value);
  return true;
}

// Valide/convertit une valeur brute en profil console supporte.
bool parseConsoleProfile(const uint8_t profileValue, ConsoleProfile& profile) {
  return parseConsoleProfileValue(profileValue, profile);
}

// Nom de sonde affiche en HTML/JSON : caracteres restreints pour eviter toute injection.
bool isValidSensorName(const String& name) {
  if (name.length() > 16) {
    return false;
  }
  for (unsigned int i = 0; i < name.length(); ++i) {
    const char c = name[i];
    const bool allowed = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                         c == ' ' || c == '-' || c == '_';
    if (!allowed) {
      return false;
    }
  }
  return true;
}

// Prefixe de stockage NVS des seuils associes au profil.
const char* getProfileStoragePrefix(ConsoleProfile profile) {
  return getConsoleProfileStoragePrefix(profile);
}

// Applique la configuration profil recue en POST.
// Ecrit en NVS, met a jour AppState et retourne un message utilisateur.
String applyProfileSettings(WebServer& server) {
  uint8_t profileRaw = 0;
  if (!parseUInt8Arg(server.arg("profile"), profileRaw)) {
    return String("ERR:Profil invalide.");
  }

  ConsoleProfile selectedProfile = DEFAULT_CONSOLE_PROFILE;
  if (!parseConsoleProfile(profileRaw, selectedProfile)) {
    return String("ERR:Profil non supporte.");
  }

  // PWM : Contrôle de cohérence des seuils de vitesse du ventilateur.
  float idleCoolMax = 0.0f;
  float gameCoolMax = 0.0f;
  float gameHotMax = 0.0f;
  logger::debugf("applyProfileSettings: profile=%d, idle_cool_max=%s, game_cool_max=%s, game_hot_max=%s",
      static_cast<uint8_t>(selectedProfile),
      server.arg("idle_cool_max").c_str(),
      server.arg("game_cool_max").c_str(),
      server.arg("game_hot_max").c_str());

  if (!parseFloatArg(server.arg("idle_cool_max"), idleCoolMax)) {
    logger::errorf("applyProfileSettings: failed to parse idle_cool_max=%s", server.arg("idle_cool_max").c_str());
    return String("ERR:IDLE_COOL_MAX invalide.");
  }
  if (!parseFloatArg(server.arg("game_cool_max"), gameCoolMax)) {
    logger::errorf("applyProfileSettings: failed to parse game_cool_max=%s", server.arg("game_cool_max").c_str());
    return String("ERR:GAME_COOL_MAX invalide.");
  }
  if (!parseFloatArg(server.arg("game_hot_max"), gameHotMax)) {
    logger::errorf("applyProfileSettings: failed to parse game_hot_max=%s", server.arg("game_hot_max").c_str());
    return String("ERR:GAME_HOT_MAX invalide.");
  }

  idleCoolMax = constrain(idleCoolMax, THRESHOLD_MIN, THRESHOLD_MAX);
  gameCoolMax = constrain(gameCoolMax, THRESHOLD_MIN, THRESHOLD_MAX);
  gameHotMax = constrain(gameHotMax, THRESHOLD_MIN, THRESHOLD_MAX);

  if (!(idleCoolMax <= gameCoolMax && gameCoolMax <= gameHotMax)) {
    logger::errorf("applyProfileSettings: invalid order: idle_cool_max=%f, game_cool_max=%f, game_hot_max=%f",
        idleCoolMax, gameCoolMax, gameHotMax);
    return String("ERR:Ordre invalide, attendu: IDLE_COOL_MAX <= GAME_COOL_MAX <= GAME_HOT_MAX.");
  }

  // TEMPERATURE : Contrôle de cohérence des seuils de température.
  float temperatureIdle = 0.0f;
  float temperatureMax = 0.0f;
  auto& state = getAppState();
  logger::debugf("applyProfileSettings: profile=%d, temperature_idle=%s, temperature_max=%s",
      static_cast<uint8_t>(selectedProfile),
      server.arg("temperature_idle").c_str(),
      server.arg("temperature_max").c_str());

  if (!parseFloatArg(server.arg("temperature_idle"), temperatureIdle)) {
    logger::errorf("applyProfileSettings: failed to parse temperature_idle=%s", server.arg("temperature_idle").c_str());
    return String("ERR:TEMPERATURE_IDLE invalide.");
  }
  if (!parseFloatArg(server.arg("temperature_max"), temperatureMax)) {
    logger::errorf("applyProfileSettings: failed to parse temperature_max=%s", server.arg("temperature_max").c_str());
    return String("ERR:TEMPERATURE_MAX invalide.");
  }

  temperatureIdle = constrain(temperatureIdle, TEMPERATURE_MIN, TEMPERATURE_MAX);
  temperatureMax = constrain(temperatureMax, TEMPERATURE_MIN, TEMPERATURE_MAX);

  if (temperatureIdle > temperatureMax) {
    logger::errorf("applyProfileSettings: invalid order: temperature_idle=%f, temperature_max=%f",
        temperatureIdle, temperatureMax);
    return String("ERR:Ordre invalide, attendu: TEMPERATURE_IDLE <= TEMPERATURE_MAX.");
  }

  // ICONE : Contrôle de cohérence du mode d'affichage des icones.
  uint8_t iconMode = 1;
  logger::debugf("applyProfileSettings: profile=%d, icon_mode=%s",
      static_cast<uint8_t>(selectedProfile),
      server.arg("icon_mode").c_str());
  if (!parseUInt8Arg(server.arg("icon_mode"), iconMode) || iconMode > 1) {
    logger::errorf("applyProfileSettings: failed to parse icon_mode=%s", server.arg("icon_mode").c_str());
    return String("ERR:Mode image invalide.");
  }

  // SONDES : valide tout avant de modifier l'etat runtime.
  SensorConfig newSensors[2];
  for (uint8_t i = 0; i < 2; ++i) {
    const String prefixArg = String("sensor") + String(i + 1);
    String address = server.arg(prefixArg + "_address");
    address.trim();
    address.toUpperCase();
    String name = server.arg(prefixArg + "_name");
    name.trim();

    if (address.length() > 0 && !temperature_manager::isValidAddress(address)) {
      return String("ERR:Adresse sonde ") + String(i + 1) + " invalide.";
    }
    if (!isValidSensorName(name)) {
      return String("ERR:Nom sonde ") + String(i + 1) + " invalide (lettres, chiffres, espace, - et _ ; 16 max).";
    }

    float idle = temperatureIdle;
    float maxValue = temperatureMax;
    if (server.hasArg(prefixArg + "_idle") && !parseFloatArg(server.arg(prefixArg + "_idle"), idle)) {
      return String("ERR:Seuil IDLE sonde ") + String(i + 1) + " invalide.";
    }
    if (server.hasArg(prefixArg + "_max") && !parseFloatArg(server.arg(prefixArg + "_max"), maxValue)) {
      return String("ERR:Seuil MAX sonde ") + String(i + 1) + " invalide.";
    }
    idle = constrain(idle, TEMPERATURE_MIN, TEMPERATURE_MAX);
    maxValue = constrain(maxValue, TEMPERATURE_MIN, TEMPERATURE_MAX);
    if (idle > maxValue) {
      return String("ERR:Sonde ") + String(i + 1) + ": seuil IDLE doit etre <= seuil MAX.";
    }

    newSensors[i].address = address;
    newSensors[i].enabled = address.length() > 0;
    newSensors[i].name = name.length() > 0 ? name : String("sonde") + String(i + 1);
    newSensors[i].temperatureIdleThreshold = idle;
    newSensors[i].temperatureMaxThreshold = maxValue;
  }
  if (newSensors[0].enabled && newSensors[0].address == newSensors[1].address) {
    return String("ERR:Les deux sondes ne peuvent pas avoir la meme adresse.");
  }

  for (uint8_t i = 0; i < 2; ++i) {
    state.sensorConfig[i].address = newSensors[i].address;
    state.sensorConfig[i].enabled = newSensors[i].enabled;
    state.sensorConfig[i].name = newSensors[i].name;
    state.sensorConfig[i].temperatureIdleThreshold = newSensors[i].temperatureIdleThreshold;
    state.sensorConfig[i].temperatureMaxThreshold = newSensors[i].temperatureMaxThreshold;
  }
  const uint8_t profileIndex = static_cast<uint8_t>(selectedProfile);
  state.profileThresholds[profileIndex].temperatureIdle = temperatureIdle;
  state.profileThresholds[profileIndex].temperatureMax = temperatureMax;

  Preferences preferences;
  if (!preferences.begin("ps5fan", false)) {
    return String("ERR:Impossible d'ouvrir la configuration.");
  }

  const char* prefix = getProfileStoragePrefix(selectedProfile);
  char keyIdleCool[16];
  // Cles NVS historiques conservees: "_i_hot" = gameCoolMax, "_g_cool" = gameHotMax.
  char keyGameCoolMax[16];
  char keyGameHotMax[16];
  snprintf(keyIdleCool, sizeof(keyIdleCool), "%s_i_cool", prefix);
  snprintf(keyGameCoolMax, sizeof(keyGameCoolMax), "%s_i_hot", prefix);
  snprintf(keyGameHotMax, sizeof(keyGameHotMax), "%s_g_cool", prefix);

  preferences.putUChar("console_profile", static_cast<uint8_t>(selectedProfile));
  preferences.putFloat(keyIdleCool, idleCoolMax);
  preferences.putFloat(keyGameCoolMax, gameCoolMax);
  preferences.putFloat(keyGameHotMax, gameHotMax);
  preferences.putUChar("icon_mode", iconMode);
  const bool savedSensorConfig = temperature_manager::saveToPreferences(preferences, selectedProfile);
  preferences.end();
  if (!savedSensorConfig) {
    logger::warn("[TEMP] sensor configuration was not saved fully");
  }
  logger::debugf("preferences saved: profile=%d, idle_cool_max=%f, game_cool_max=%f, game_hot_max=%f, temperature_idle=%f, temperature_max=%f, icon_mode=%d",
      static_cast<uint8_t>(selectedProfile),
      idleCoolMax,
      gameCoolMax,
      gameHotMax,
      temperatureIdle,
      temperatureMax,
      iconMode);

  state.activeProfile = selectedProfile;
  state.iconMode = iconMode;
  state.profileThresholds[profileIndex].idleCoolMax = idleCoolMax;
  state.profileThresholds[profileIndex].gameCoolMax = gameCoolMax;
  state.profileThresholds[profileIndex].gameHotMax = gameHotMax;
  state.dutyMax = 0.0f;

  return String("Parametres sauvegardes. Redemarrage de l'ESP32-C3...");
}

// Applique la configuration de la page console (niveau log + refresh).
String applyConsoleSettings(WebServer& server) {
  uint8_t logLevelRaw = 0;
  if (!parseUInt8Arg(server.arg("log_level"), logLevelRaw)) {
    return String("ERR:Niveau de log invalide.");
  }

  const uint8_t maxLogLevel = static_cast<uint8_t>(log_config::LogLevel::ERROR);
  if (logLevelRaw > maxLogLevel) {
    return String("ERR:Niveau de log non supporte.");
  }

  uint8_t refreshRaw = 0;
  if (!parseUInt8Arg(server.arg("refresh_seconds"), refreshRaw)) {
    return String("ERR:Refresh invalide.");
  }
  if (!log_config::isSupportedWebRefreshSeconds(refreshRaw)) {
    return String("ERR:Refresh supporte: 1s, 2s, 5s ou 10s.");
  }

  if (!saveWebLogSettings(logLevelRaw, static_cast<uint16_t>(refreshRaw))) {
    return String("ERR:Echec de sauvegarde des parametres de logs.");
  }

  return String("Parametres des logs sauvegardes. Redemarrage de l'ESP32-C3...");
}

// Construit l'etat courant des sondes de temperature pour le Web.
String buildTemperatureStatusJson() {
  auto& state = getAppState();
  String payload;
  payload.reserve(384);
  payload += "{";

  for (uint8_t i = 0; i < 2; ++i) {
    if (i > 0) {
      payload += ",";
    }

    payload += "\"";
    payload += String(i);
    payload += "\":{";
    payload += "\"address\":\"";
    payload += state.sensorConfig[i].address;
    payload += "\",\"name\":\"";
    payload += state.sensorConfig[i].name;
    payload += "\",\"enabled\":";
    payload += state.sensorConfig[i].enabled ? "true" : "false";
    payload += ",\"detected\":";
    payload += state.sensorConfig[i].detected ? "true" : "false";
    payload += ",\"temperature\":";
    if (isnan(state.sensorConfig[i].currentTemperature)) {
      payload += "null";
    } else {
      payload += String(state.sensorConfig[i].currentTemperature, 1);
    }
    payload += "}";
  }

  payload += "}";
  return payload;
}

// Remise a zero complete des reglages profil (NVS) et de l'etat runtime associe.
bool resetProfileSettings() {
  const bool persistedResetOk = resetProfilePageSettingsToDefaults();

  auto& state = getAppState();
  state.currentPage = DisplayPage::SimplePwm;
  state.activeProfile = DEFAULT_CONSOLE_PROFILE;
  state.thresholdOffset = 0.0f;
  state.iconMode = 1;
  state.webLogLevel = static_cast<uint8_t>(log_config::CURRENT_LOG_LEVEL);
  state.webLogRefreshSeconds = log_config::LOG_WEB_REFRESH_SECONDS;
  state.dutyFiltered = 0.0f;
  state.frequencyFiltered = 0.0f;
  state.pwmDetected = false;
  state.isFirstSample = true;
  state.signalLost = false;
  state.dutyMax = 0.0f;

  for (uint8_t i = 0; i < CONSOLE_PROFILE_COUNT; ++i) {
    const ProfileThresholdPreset preset = getDefaultProfileThresholdPreset(static_cast<ConsoleProfile>(i));
    state.profileThresholds[i].idleCoolMax = preset.idleCoolMax;
    state.profileThresholds[i].gameCoolMax = preset.gameCoolMax;
    state.profileThresholds[i].gameHotMax = preset.gameHotMax;
    state.profileThresholds[i].temperatureIdle = preset.temperatureIdle;
    state.profileThresholds[i].temperatureMax = preset.temperatureMax;
  }

  logger::setLevel(static_cast<logger::LogLevel>(state.webLogLevel));
  resetHistoryBuffer();

  return persistedResetOk;
}

// Construit la reponse JSON de l'endpoint /api/history.
String buildHistoryJsonPayload() {
  auto& state = getAppState();
  String payload;
  payload.reserve(640);

  payload += "{\"current\":";
  payload += String(state.dutyFiltered, 1);
  payload += ",\"max\":";
  payload += String(state.dutyMax, 1);
  payload += ",\"frequency_hz\":";
  payload += String(state.frequencyFiltered, 1);
  payload += ",\"history\":[";

  for (uint8_t i = 0; i < GRAPH_VISIBLE_POINTS; ++i) {
    if (i > 0) {
      payload += ",";
    }

    const uint8_t historyIndex = GRAPH_POINTS - GRAPH_VISIBLE_POINTS + i;
    const float value = state.dutyHistory[historyIndex];
    if (value < 0.0f) {
      payload += "null";
    } else {
      payload += String(value, 1);
    }
  }

  payload += "],\"temperatures\":";
  payload += String(buildTemperatureStatusJson().c_str());
  payload += "}";
  return payload;
}

// Ajoute {"used":..,"total":..,"pct":..} (octets) au JSON.
void appendMemoryEntry(String& out, const char* name, uint32_t used, uint32_t total) {
  out += "\"";
  out += name;
  out += "\":{\"used\":";
  out += String(static_cast<unsigned long>(used));
  out += ",\"total\":";
  out += String(static_cast<unsigned long>(total));
  out += ",\"pct\":";
  out += String(total > 0 ? (100.0f * used / total) : 0.0f, 1);
  out += "}";
}

// Etat memoire: RAM (tas), flash programme et partition LittleFS.
String buildMemoryJson() {
  const uint32_t heapTotal = ESP.getHeapSize();
  const uint32_t heapFree = ESP.getFreeHeap();
  const uint32_t sketchUsed = ESP.getSketchSize();
  const uint32_t sketchTotal = sketchUsed + ESP.getFreeSketchSpace();

  String json = "{";
  appendMemoryEntry(json, "heap", heapTotal - heapFree, heapTotal);
  json += ",\"heap_min_free\":";
  json += String(static_cast<unsigned long>(ESP.getMinFreeHeap()));
  json += ",\"heap_max_block\":";
  json += String(static_cast<unsigned long>(ESP.getMaxAllocHeap()));
  json += ",";
  appendMemoryEntry(json, "flash", sketchUsed, sketchTotal);
  json += ",";
  appendMemoryEntry(json, "fs", LittleFS.usedBytes(), LittleFS.totalBytes());
  json += "}";
  return json;
}

// Construit la reponse JSON de l'endpoint /api/status (etat live complet).
String buildStatusJsonPayload() {
  auto& state = getAppState();
  const char* profileName = getConsoleProfileName(state.activeProfile);

  String payload;
  payload.reserve(1024);
  payload += "{\"uptime_s\":";
  payload += String(static_cast<unsigned long>(millis() / 1000UL));
  payload += ",\"profile\":\"";
  payload += profileName;
  payload += "\",\"pwm_detected\":";
  payload += state.pwmDetected ? "true" : "false";
  payload += ",\"duty\":";
  payload += String(state.dutyFiltered, 1);
  payload += ",\"duty_max\":";
  payload += String(state.dutyMax, 1);
  payload += ",\"frequency_hz\":";
  payload += String(state.frequencyFiltered, 1);
  payload += ",\"wifi_connected\":";
  payload += state.wifiConnected ? "true" : "false";
  payload += ",\"alerts\":{\"overheat\":";
  payload += state.alertOverheat ? "true" : "false";
  payload += ",\"fan_high\":";
  payload += state.alertFanHigh ? "true" : "false";
  payload += ",\"sensor\":";
  payload += String(static_cast<int>(state.alertSensorIndex));
  payload += ",\"temperature\":";
  if (isnan(state.alertTemperature)) {
    payload += "null";
  } else {
    payload += String(state.alertTemperature, 1);
  }
  payload += "},\"memory\":";
  payload += buildMemoryJson();
  payload += ",\"temperatures\":";
  payload += buildTemperatureStatusJson();
  payload += "}";
  return payload;
}

// Serialise une valeur en dixiemes (kInvalid -> null).
void appendTenths(String& out, int16_t tenths) {
  if (tenths == session_stats::kInvalid) {
    out += "null";
  } else {
    out += String(tenths / 10.0f, 1);
  }
}

// Envoie un tableau JSON de la serie [first, first+count) par blocs pour limiter la RAM.
void sendTrendSeries(WebServer& server, const char* name, int8_t sensor, uint16_t first, uint16_t count) {
  String chunk;
  chunk.reserve(256);
  chunk += "\"";
  chunk += name;
  chunk += "\":[";
  for (uint16_t i = 0; i < count; ++i) {
    if (i > 0) {
      chunk += ",";
    }
    const int16_t value = sensor < 0 ? session_stats::trendDuty(first + i)
                                     : session_stats::trendTemp(static_cast<uint8_t>(sensor), first + i);
    appendTenths(chunk, value);
    if (chunk.length() > 200) {
      server.sendContent(chunk);
      chunk = "";
    }
  }
  chunk += "]";
  server.sendContent(chunk);
}

// Min/max/moyenne d'une serie sur la fenetre demandee.
String buildSeriesStatsJson(int8_t sensor, uint16_t first, uint16_t count) {
  int32_t minV = INT32_MAX;
  int32_t maxV = INT32_MIN;
  int64_t sum = 0;
  uint16_t valid = 0;
  for (uint16_t i = 0; i < count; ++i) {
    const int16_t value = sensor < 0 ? session_stats::trendDuty(first + i)
                                     : session_stats::trendTemp(static_cast<uint8_t>(sensor), first + i);
    if (value == session_stats::kInvalid) {
      continue;
    }
    minV = min<int32_t>(minV, value);
    maxV = max<int32_t>(maxV, value);
    sum += value;
    ++valid;
  }

  if (valid == 0) {
    return String("null");
  }
  String json = "{\"min\":";
  json += String(minV / 10.0f, 1);
  json += ",\"max\":";
  json += String(maxV / 10.0f, 1);
  json += ",\"avg\":";
  json += String(static_cast<float>(sum) / valid / 10.0f, 1);
  json += "}";
  return json;
}

String buildSessionJson(const session_stats::Summary& summary) {
  if (!summary.valid) {
    return String("null");
  }
  String json = "{\"active\":";
  json += summary.active ? "true" : "false";
  json += ",\"duration_s\":";
  json += String(static_cast<unsigned long>(summary.durationS));
  json += ",\"avg_duty\":";
  json += String(summary.avgDuty, 1);
  json += ",\"max_duty\":";
  json += String(summary.maxDuty, 1);
  json += ",\"hot_s\":";
  json += String(static_cast<unsigned long>(summary.hotSeconds));
  for (uint8_t i = 0; i < 2; ++i) {
    json += ",\"max_t";
    json += String(i);
    json += "\":";
    if (isnan(summary.maxTemp[i])) {
      json += "null";
    } else {
      json += String(summary.maxTemp[i], 1);
    }
  }
  json += "}";
  return json;
}

// Reponse /api/live: tendance + statistiques de fenetre + resume de session (envoi par blocs).
void sendLiveJson(WebServer& server) {
  uint16_t rangeSeconds = 900;
  if (server.hasArg("range")) {
    const long requested = server.arg("range").toInt();
    if (requested == 300 || requested == 900 || requested == 1800 || requested == 3600) {
      rangeSeconds = static_cast<uint16_t>(requested);
    }
  }

  const uint16_t total = session_stats::trendCount();
  const uint16_t wanted = rangeSeconds / LIVE_SAMPLE_S;
  const uint16_t count = total < wanted ? total : wanted;
  const uint16_t first = total - count;

  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  String head = "{\"step_s\":";
  head += String(static_cast<unsigned long>(LIVE_SAMPLE_S));
  head += ",\"count\":";
  head += String(count);
  head += ",";
  server.sendContent(head);
  sendTrendSeries(server, "duty", -1, first, count);
  server.sendContent(",");
  sendTrendSeries(server, "t0", 0, first, count);
  server.sendContent(",");
  sendTrendSeries(server, "t1", 1, first, count);

  String tail = ",\"stats\":{\"duty\":";
  tail += buildSeriesStatsJson(-1, first, count);
  tail += ",\"t0\":";
  tail += buildSeriesStatsJson(0, first, count);
  tail += ",\"t1\":";
  tail += buildSeriesStatsJson(1, first, count);
  tail += "},\"session\":{\"current\":";
  tail += buildSessionJson(session_stats::currentSession());
  tail += ",\"last\":";
  tail += buildSessionJson(session_stats::lastSession());
  tail += "}}";
  server.sendContent(tail);
  server.sendContent("");
}

// Verifie l'authentification en memoire et/ou par cookie HTTP.
bool isAuthorized(WebServer& server) {
  const String cookieHeader = server.header("Cookie");
  const String expectedCookie = getAuthCookieValue();
  const bool authorizedByCookie = cookieHeader.indexOf(expectedCookie) != -1;

  return gIsAuthenticated || authorizedByCookie;
}

}  // namespace

void web_ui::initializeRoutes(WebServer& server) {
  // Initialise LittleFS avant de declarer les routes qui servent templates/assets.
  web_pages::initializeStorage();

  server.on("/assets/style.css", HTTP_GET, [&server]() {
    if (web_storage::sendStaticFile(server, "/assets/style.css", "text/css", "public, max-age=3600")) {
      return;
    }

    server.sendHeader("Cache-Control", "public, max-age=3600");
    server.send(200, "text/css", web_pages::getSharedCss());
  });

  server.on("/", HTTP_GET, [&server]() {
    logger::debugf("GET / - cookie header='%s'", server.header("Cookie").c_str());
    if (!isAuthorized(server)) {
      logger::debug("GET / unauthorized -> serve login page");
      server.sendHeader("Cache-Control", "no-store");
      server.send(200, "text/html", web_pages::getLoginPage());
      return;
    }

    logger::debug("GET / authorized -> serving home page");
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html", web_pages::getHomePage(getAppState()));
  });

  server.on("/login", HTTP_GET, [&server]() {
    logger::debugf("GET /login - cookie header='%s'", server.header("Cookie").c_str());
    if (isAuthorized(server)) {
      logger::debug("GET /login authorized -> serve home page");
      server.sendHeader("Cache-Control", "no-store");
      server.send(200, "text/html", web_pages::getHomePage(getAppState()));
      return;
    }

    logger::debug("GET /login unauthenticated -> serving login page");
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html", web_pages::getLoginPage());
  });

  server.on("/login", HTTP_POST, [&server]() {
    String enteredPassword = server.hasArg("password") ? server.arg("password") : String();
    enteredPassword.trim();
    const String configuredPassword = String(web_config::WEB_PASSWORD);

    logger::debug("POST /login");

    if (enteredPassword == configuredPassword) {
      const String cookieValue = getAuthCookieValue();
      gIsAuthenticated = true;
      logger::debugf("password OK, setting cookie '%s'", cookieValue.c_str());
      server.sendHeader("Set-Cookie", String(web_config::WEB_SESSION_COOKIE) + "=authorized; Path=/; Max-Age=3600");
      server.sendHeader("Cache-Control", "no-store");
      server.sendHeader("Location", "/");
      server.send(303, "text/plain", "");
      return;
    }

    gIsAuthenticated = false;
    logger::debug("password KO");
    server.sendHeader("Set-Cookie", String(web_config::WEB_SESSION_COOKIE) + "=; Path=/; Max-Age=0");
    server.sendHeader("Cache-Control", "no-store");
    server.send(401, "text/html", web_pages::getLoginPage("Mot de passe invalide"));
  });

  server.on("/history", HTTP_GET, [&server]() {
    logger::debugf("GET /history - cookie header='%s'", server.header("Cookie").c_str());
    if (!isAuthorized(server)) {
      logger::debug("GET /history unauthorized -> redirect to /login");
      redirectTo(server, "/login");
      return;
    }

    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html", web_pages::getHistoryPage());
  });

  server.on("/api/temperature/scan", HTTP_GET, [&server]() {
    logger::debug("GET /api/temperature/scan");
    if (!isAuthorized(server)) {
      server.sendHeader("Cache-Control", "no-store");
      server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
      return;
    }

    temperature_manager::scanSensors();
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", temperature_manager::getDetectedSensorsJson());
  });

  server.on("/api/temperature/status", HTTP_GET, [&server]() {
    if (!isAuthorized(server)) {
      server.sendHeader("Cache-Control", "no-store");
      server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
      return;
    }

    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", buildTemperatureStatusJson());
  });

  server.on("/api/status", HTTP_GET, [&server]() {
    if (!isAuthorized(server)) {
      server.sendHeader("Cache-Control", "no-store");
      server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
      return;
    }

    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", buildStatusJsonPayload());
  });

  server.on("/api/live", HTTP_GET, [&server]() {
    if (!isAuthorized(server)) {
      server.sendHeader("Cache-Control", "no-store");
      server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
      return;
    }

    sendLiveJson(server);
  });

  server.on("/live", HTTP_GET, [&server]() {
    if (!isAuthorized(server)) {
      redirectTo(server, "/login");
      return;
    }

    if (web_storage::sendStaticFile(server, "/pages/live.html", "text/html", "no-store")) {
      return;
    }

    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html", web_pages::getLivePageFallback());
  });

  server.on("/api/history", HTTP_GET, [&server]() {
    if (!isAuthorized(server)) {
      logger::debug("GET /api/history unauthorized -> 401");
      server.sendHeader("Cache-Control", "no-store");
      server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
      return;
    }

    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", buildHistoryJsonPayload());
  });

  server.on("/console", HTTP_GET, [&server]() {
    logger::debugf("GET /console - cookie header='%s'", server.header("Cookie").c_str());
    if (!isAuthorized(server)) {
      logger::debug("GET /console unauthorized -> redirect to /login");
      redirectTo(server, "/login");
      return;
    }

    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html", getConsolePage());
  });

  server.on("/console/settings", HTTP_POST, [&server]() {
    logger::debugf("POST /console/settings - cookie header='%s'", server.header("Cookie").c_str());
    if (!isAuthorized(server)) {
      logger::debug("POST /console/settings unauthorized -> redirect to /login");
      redirectTo(server, "/login");
      return;
    }

    const String statusMessage = applyConsoleSettings(server);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html", getConsolePage(statusMessage));
    if (!statusMessage.startsWith("ERR:")) {
      delay(300);
      ESP.restart();
    }
  });

  server.on("/profile", HTTP_GET, [&server]() {
    logger::debugf("GET /profile - cookie header='%s'", server.header("Cookie").c_str());
    if (!isAuthorized(server)) {
      logger::debug("GET /profile unauthorized -> redirect to /login");
      redirectTo(server, "/login");
      return;
    }

    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html", web_pages::getProfilePage(getAppState()));
  });

  server.on("/profile/apply", HTTP_GET, [&server]() {
    logger::debugf("GET /profile/apply - cookie header='%s'", server.header("Cookie").c_str());
    if (!isAuthorized(server)) {
      logger::debug("GET /profile/apply unauthorized -> redirect to /login");
      redirectTo(server, "/login");
      return;
    }

    logger::debug("GET /profile/apply -> redirect to /profile");
    redirectTo(server, "/profile");
  });

  server.on("/profile/apply", HTTP_POST, [&server]() {
    logger::debugf("POST /profile/apply - cookie header='%s'", server.header("Cookie").c_str());
    if (!isAuthorized(server)) {
      logger::debug("POST /profile/apply unauthorized -> redirect to /login");
      redirectTo(server, "/login");
      return;
    }

    const String statusMessage = applyProfileSettings(server);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html", web_pages::getProfilePage(getAppState(), statusMessage));
    if (!statusMessage.startsWith("ERR:")) {
      delay(1000);
      ESP.restart();
    }
  });

  server.on("/profile/reset", HTTP_GET, [&server]() {
    logger::debugf("GET /profile/reset - cookie header='%s'", server.header("Cookie").c_str());
    if (!isAuthorized(server)) {
      logger::debug("GET /profile/reset unauthorized -> redirect to /login");
      redirectTo(server, "/login");
      return;
    }

    logger::debug("GET /profile/reset -> redirect to /profile");
    redirectTo(server, "/profile");
  });

  server.on("/profile/reset", HTTP_POST, [&server]() {
    logger::debugf("POST /profile/reset - cookie header='%s'", server.header("Cookie").c_str());
    if (!isAuthorized(server)) {
      logger::debug("POST /profile/reset unauthorized -> redirect to /login");
      redirectTo(server, "/login");
      return;
    }

    const bool resetOk = resetProfileSettings();
    if (!resetOk) {
      server.sendHeader("Cache-Control", "no-store");
      server.send(200, "text/html", web_pages::getProfilePage(getAppState(), "ERR:Echec de remise a zero des parametres profil."));
      return;
    }

    // Redirige vers /profile pour eviter un futur refresh en GET /profile/reset (404).
    redirectTo(server, "/profile");
    delay(1000);
    ESP.restart();
  });

  server.onNotFound([&server]() {
    logger::debugf("onNotFound - method=%d uri='%s' cookie='%s'",
                   server.method(),
                   server.uri().c_str(),
                   server.header("Cookie").c_str());
    if (!isAuthorized(server)) {
      logger::debug("onNotFound unauthorized -> redirect to /login");
      redirectTo(server, "/login");
      return;
    }

    logger::debug("onNotFound authorized -> 404");
    server.send(404, "text/plain", "Page introuvable");
  });

  server.on("/favicon.ico", HTTP_GET, [&server]() {
    logger::debug("GET /favicon.ico -> 204");
    server.send(204, "text/plain", "");
  });
}
