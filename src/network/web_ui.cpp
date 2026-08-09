#include "web_ui.h"

#include <ESP.h>
#include <Preferences.h>
#include <stdlib.h>

#include "../app/app_state.h"
#include "../app/app_logic.h"
#include "../config/log_config.h"
#include "../config/profiles.h"
#include "../config/web_config.h"
#include "web_pages.h"
#include "web_storage.h"
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
  switch (profileValue) {
    case static_cast<uint8_t>(ConsoleProfile::PS3_FAT):
      profile = ConsoleProfile::PS3_FAT;
      return true;
    case static_cast<uint8_t>(ConsoleProfile::PS4_PRO):
      profile = ConsoleProfile::PS4_PRO;
      return true;
    case static_cast<uint8_t>(ConsoleProfile::PS5_FAT):
      profile = ConsoleProfile::PS5_FAT;
      return true;
    default:
      return false;
  }
}

// Prefixe de stockage NVS des seuils associes au profil.
const char* getProfileStoragePrefix(ConsoleProfile profile) {
  switch (profile) {
    case ConsoleProfile::PS4_PRO:
      return "p4";
    case ConsoleProfile::PS3_FAT:
      return "p3";
    case ConsoleProfile::PS5_FAT:
    default:
      return "p5";
  }
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

  float idleCoolMax = 0.0f;
  float gameCoolMax = 0.0f;
    float gameHotMax = 0.0f;
  if (!parseFloatArg(server.arg("idle_cool_max"), idleCoolMax)) {
    return String("ERR:IDLE_COOL_MAX invalide.");
  }
  if (!parseFloatArg(server.arg("game_cool_max"), gameCoolMax)) {
    return String("ERR:GAME_COOL_MAX invalide.");
  }
    if (!parseFloatArg(server.arg("game_hot_max"), gameHotMax)) {
      return String("ERR:GAME_HOT_MAX invalide.");
    }

  idleCoolMax = constrain(idleCoolMax, THRESHOLD_MIN, THRESHOLD_MAX);
  gameCoolMax = constrain(gameCoolMax, THRESHOLD_MIN, THRESHOLD_MAX);
    gameHotMax = constrain(gameHotMax, THRESHOLD_MIN, THRESHOLD_MAX);

    if (!(idleCoolMax <= gameCoolMax && gameCoolMax <= gameHotMax)) {
      return String("ERR:Ordre invalide, attendu: IDLE_COOL_MAX <= GAME_COOL_MAX <= GAME_HOT_MAX.");
  }

  uint8_t iconMode = 1;
  if (!parseUInt8Arg(server.arg("icon_mode"), iconMode) || iconMode > 1) {
    return String("ERR:Mode image invalide.");
  }

  Preferences preferences;
  if (!preferences.begin("ps5fan", false)) {
    return String("ERR:Impossible d'ouvrir la configuration.");
  }

  const char* prefix = getProfileStoragePrefix(selectedProfile);
  char keyIdleCool[16];
  char keyIdleHot[16];
  char keyGameCool[16];
  snprintf(keyIdleCool, sizeof(keyIdleCool), "%s_i_cool", prefix);
  snprintf(keyIdleHot, sizeof(keyIdleHot), "%s_i_hot", prefix);
  snprintf(keyGameCool, sizeof(keyGameCool), "%s_g_cool", prefix);

  preferences.putUChar("console_profile", static_cast<uint8_t>(selectedProfile));
  preferences.putFloat(keyIdleCool, idleCoolMax);
  preferences.putFloat(keyIdleHot, gameCoolMax);
  preferences.putFloat(keyGameCool, gameHotMax);
  preferences.putUChar("icon_mode", iconMode);
  preferences.end();

  auto& state = getAppState();
  state.activeProfile = selectedProfile;
  state.iconMode = iconMode;
  const uint8_t profileIndex = static_cast<uint8_t>(selectedProfile);
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
  if (!(refreshRaw == 1 || refreshRaw == 2 || refreshRaw == 5)) {
    return String("ERR:Refresh supporte: 1s, 2s ou 5s.");
  }

  if (!saveWebLogSettings(logLevelRaw, static_cast<uint16_t>(refreshRaw))) {
    return String("ERR:Echec de sauvegarde des parametres de logs.");
  }

  return String("Parametres des logs sauvegardes. Redemarrage de l'ESP32-C3...");
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

  const uint8_t p5 = static_cast<uint8_t>(ConsoleProfile::PS5_FAT);
  state.profileThresholds[p5].idleCoolMax = STATUS_PS5F_IDLE_COOL_MAX;
  state.profileThresholds[p5].gameCoolMax = STATUS_PS5F_GAME_COOL_MAX;
  state.profileThresholds[p5].gameHotMax = STATUS_PS5F_GAME_HOT_MAX;

  const uint8_t p4 = static_cast<uint8_t>(ConsoleProfile::PS4_PRO);
  state.profileThresholds[p4].idleCoolMax = STATUS_PS4P_IDLE_COOL_MAX;
  state.profileThresholds[p4].gameCoolMax = STATUS_PS4P_GAME_COOL_MAX;
  state.profileThresholds[p4].gameHotMax = STATUS_PS4P_GAME_HOT_MAX;

  const uint8_t p3 = static_cast<uint8_t>(ConsoleProfile::PS3_FAT);
  state.profileThresholds[p3].idleCoolMax = STATUS_PS3F_IDLE_COOL_MAX;
  state.profileThresholds[p3].gameCoolMax = STATUS_PS3F_GAME_COOL_MAX;
  state.profileThresholds[p3].gameHotMax = STATUS_PS3F_GAME_HOT_MAX;

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

  payload += "]}";
  return payload;
}

// Verifie l'authentification en memoire et/ou par cookie HTTP.
bool isAuthorized(WebServer& server) {
  const String cookieHeader = server.header("Cookie");
  const String expectedCookie = getAuthCookieValue();
  const bool authorizedByCookie = cookieHeader.indexOf(expectedCookie) != -1;

  logger::debugf("Cookie header: '%s'", cookieHeader.c_str());
  logger::debugf("Looking for cookie: '%s'", expectedCookie.c_str());
  logger::debugf("Memory auth flag: %s", gIsAuthenticated ? "true" : "false");

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

    logger::debugf("POST /login - enteredPassword='%s'", enteredPassword.c_str());
    logger::debugf("configuredPassword='%s'", configuredPassword.c_str());
    logger::debugf("POST /login - cookie header='%s'", server.header("Cookie").c_str());

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

  server.on("/api/history", HTTP_GET, [&server]() {
    logger::debugf("GET /api/history - cookie header='%s'", server.header("Cookie").c_str());
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
      delay(300);
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
    delay(300);
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
