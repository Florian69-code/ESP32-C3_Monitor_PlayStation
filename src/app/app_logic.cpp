#include "app_logic.h"

#include <Preferences.h>
#include <WebServer.h>

#include "../config.h"
#include "../config/web_config.h"
#include "../display_ui.h"
#include "../network/wifi_manager.h"
#include "../pwm_sampler.h"
#include "../utils/logger.h"

namespace {
Preferences preferences;
PwmSampler pwmSampler(PWM_INPUT_PIN);
WebServer webServer(web_config::WEB_PORT);

bool buttonWasPressed = false;
bool longPressHandled = false;
uint32_t buttonPressStartMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastGraphMs = 0;
uint32_t lastValidSignalMs = 0;

// Les cles NVS sont limitees a 15 caracteres max sur ESP32.
constexpr const char* kThresholdOffsetKey = "thr_offset";

// Convertit un profil enum en index de tableau sans logique metier supplementaire.
uint8_t profileToIndex(ConsoleProfile profile) {
  return static_cast<uint8_t>(profile);
}

// Retourne le prefixe de cle NVS associe au profil.
// Exemples: p5_i_cool, p4_g_cool, p3_i_hot.
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

// Charge les seuils persistants d'un profil, avec fallback sur presets par defaut.
void loadThresholdPresetForProfile(ConsoleProfile profile) {
  const ProfileThresholdPreset defaults = getDefaultProfileThresholdPreset(profile);
  const char* prefix = getProfileStoragePrefix(profile);
  const uint8_t idx = profileToIndex(profile);

  char keyIdleCool[16];
  char keyIdleHot[16];
  char keyGameCool[16];
  snprintf(keyIdleCool, sizeof(keyIdleCool), "%s_i_cool", prefix);
  snprintf(keyIdleHot, sizeof(keyIdleHot), "%s_i_hot", prefix);
  snprintf(keyGameCool, sizeof(keyGameCool), "%s_g_cool", prefix);

  auto& thresholds = getAppState().profileThresholds[idx];
  thresholds.idleCoolMax = preferences.getFloat(keyIdleCool, defaults.idleCoolMax);
  thresholds.gameCoolMax = preferences.getFloat(keyIdleHot, defaults.gameCoolMax);
  thresholds.gameHotMax = preferences.getFloat(keyGameCool, defaults.gameHotMax);
}

bool buttonIsPressed();

// Affiche la sequence de boot pendant BOOT_SCREEN_MS.
// L'utilisateur peut la quitter immediatement via le bouton.
void waitOnBootScreen() {
  const uint32_t startMs = millis();
  uint8_t frame = 0;

  while (millis() - startMs < BOOT_SCREEN_MS) {
    if (buttonIsPressed()) {
      break;
    }

    drawProfileBootAnimation(frame++);
    delay(80);
  }

  drawBootScreen();
}

// Lit le bouton en tenant compte du mode actif bas/haut.
bool buttonIsPressed() {
  const bool level = digitalRead(BUTTON_PIN);
  return BUTTON_ACTIVE_LOW ? !level : level;
}

// Push d'un point d'historique avec decalage FIFO et mise a jour du max.
void addHistoryPoint(float value) {
  auto& state = getAppState();
  for (uint8_t i = 0; i < GRAPH_POINTS - 1; ++i) {
    state.dutyHistory[i] = state.dutyHistory[i + 1];
  }
  state.dutyHistory[GRAPH_POINTS - 1] = value;

  if (value > state.dutyMax) {
    state.dutyMax = value;
  }
}

// Persiste la page OLED courante.
void syncPagePreference() {
  preferences.putUChar("page", static_cast<uint8_t>(getAppState().currentPage));
}

// Charge tous les reglages sauvegardes depuis Preferences.
// Applique aussi les bornes de securite pour eviter des valeurs incoherentes.
void loadPersistedSettings() {
  const uint8_t savedPage = preferences.getUChar("page", static_cast<uint8_t>(DisplayPage::SimplePwm));
  if (savedPage < static_cast<uint8_t>(DisplayPage::Count)) {
    getAppState().currentPage = static_cast<DisplayPage>(savedPage);
  }

  const uint8_t savedProfile = preferences.getUChar("console_profile", static_cast<uint8_t>(DEFAULT_CONSOLE_PROFILE));
  switch (savedProfile) {
    case static_cast<uint8_t>(ConsoleProfile::PS4_PRO):
      getAppState().activeProfile = ConsoleProfile::PS4_PRO;
      break;
    case static_cast<uint8_t>(ConsoleProfile::PS3_FAT):
      getAppState().activeProfile = ConsoleProfile::PS3_FAT;
      break;
    case static_cast<uint8_t>(ConsoleProfile::PS5_FAT):
    default:
      getAppState().activeProfile = ConsoleProfile::PS5_FAT;
      break;
  }

  getAppState().thresholdOffset = preferences.getFloat(kThresholdOffsetKey, 0.0f);

  loadThresholdPresetForProfile(ConsoleProfile::PS5_FAT);
  loadThresholdPresetForProfile(ConsoleProfile::PS4_PRO);
  loadThresholdPresetForProfile(ConsoleProfile::PS3_FAT);

  const uint8_t storedIconMode = preferences.getUChar("icon_mode", 1);
  getAppState().iconMode = storedIconMode > 1 ? 1 : storedIconMode;

  const uint8_t maxLogLevel = static_cast<uint8_t>(log_config::LogLevel::ERROR);
  const uint8_t storedLogLevel = preferences.getUChar(
      "log_level",
      static_cast<uint8_t>(log_config::CURRENT_LOG_LEVEL));
  getAppState().webLogLevel = storedLogLevel <= maxLogLevel
                                  ? storedLogLevel
                                  : static_cast<uint8_t>(log_config::CURRENT_LOG_LEVEL);

  const uint16_t storedRefreshSeconds = preferences.getUShort(
      "log_refresh",
      log_config::LOG_WEB_REFRESH_SECONDS);
  if (storedRefreshSeconds == 1 || storedRefreshSeconds == 2 || storedRefreshSeconds == 5) {
    getAppState().webLogRefreshSeconds = storedRefreshSeconds;
  } else {
    getAppState().webLogRefreshSeconds = log_config::LOG_WEB_REFRESH_SECONDS;
  }

  logger::setLevel(static_cast<logger::LogLevel>(getAppState().webLogLevel));
}

// Configure la LED de feedback thermique en sortie.
void initializeLedState() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
}
}

void initializeApp() {
  if (SERIAL_DEBUG_ENABLED) {
    Serial.begin(SERIAL_BAUDRATE);
  }

  logger::initialize();

  pinMode(BUTTON_PIN, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  initializeLedState();

  preferences.begin("ps5fan", false);
  loadPersistedSettings();

  display.begin();
  display.setContrast(OLED_CONTRAST);
  drawBootScreen();
  waitOnBootScreen();

  resetHistoryBuffer();
  getAppState().wifiConnected = wifi_manager::initializeWifi();
  if (!getAppState().wifiConnected) {
    drawWifiErrorScreen();
    delay(1200);
  }

  wifi_manager::initializeWebServer(webServer);
  webServer.begin();
  pwmSampler.begin();
}

void handleShortPress() {
  auto& state = getAppState();

  const uint8_t nextPage = (static_cast<uint8_t>(state.currentPage) + 1) % static_cast<uint8_t>(DisplayPage::Count);
  state.currentPage = static_cast<DisplayPage>(nextPage);
  syncPagePreference();
}

void handleLongPress() {
  auto& state = getAppState();
  state.currentPage = DisplayPage::SimplePwm;
  syncPagePreference();
}

void updateButtonState() {
  const bool pressed = buttonIsPressed();
  const uint32_t nowMs = millis();

  if (!pressed && buttonWasPressed) {
    const uint32_t duration = nowMs - buttonPressStartMs;

    if (duration >= BUTTON_DEBOUNCE_MS && !longPressHandled) {
      if (duration >= BUTTON_LONG_PRESS_MS) {
        handleLongPress();
      } else {
        handleShortPress();
      }
    }

    buttonWasPressed = false;
    longPressHandled = false;
    return;
  }

  if (pressed && !buttonWasPressed) {
    buttonWasPressed = true;
    buttonPressStartMs = nowMs;
    longPressHandled = false;
  }

  if (pressed && buttonWasPressed && !longPressHandled && nowMs - buttonPressStartMs >= BUTTON_LONG_PRESS_MS) {
    longPressHandled = true;
    handleLongPress();
  }
}

void updateLedState() {
  static bool ledState = false;
  static uint32_t lastBlink = 0;
  auto& state = getAppState();

  if (!LED_TEMP_ENABLED) {
    digitalWrite(LED_PIN, HIGH);
    return;
  }

  const float gameHotMax = getActiveGameHotMax();
  if (state.dutyFiltered > gameHotMax + 10.0f) {
    if (millis() - lastBlink > 100) {
      lastBlink = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);
    }
  } else if (state.dutyFiltered > gameHotMax) {
    if (millis() - lastBlink > 500) {
      lastBlink = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);
    }
  } else {
    digitalWrite(LED_PIN, HIGH);
  }
}

bool saveWebLogSettings(const uint8_t logLevel, const uint16_t refreshSeconds) {
  const uint8_t maxLogLevel = static_cast<uint8_t>(log_config::LogLevel::ERROR);
  if (logLevel > maxLogLevel) {
    return false;
  }

  if (!(refreshSeconds == 1 || refreshSeconds == 2 || refreshSeconds == 5)) {
    return false;
  }

  const size_t wroteLevel = preferences.putUChar("log_level", logLevel);
  const size_t wroteRefresh = preferences.putUShort("log_refresh", refreshSeconds);
  if (wroteLevel == 0 || wroteRefresh == 0) {
    return false;
  }

  auto& state = getAppState();
  state.webLogLevel = logLevel;
  state.webLogRefreshSeconds = refreshSeconds;
  logger::setLevel(static_cast<logger::LogLevel>(logLevel));
  return true;
}

bool resetProfilePageSettingsToDefaults() {
  bool writeOk = true;

  auto writeAndVerifyUChar = [&](const char* key, const uint8_t value) {
    const size_t written = preferences.putUChar(key, value);
    const uint8_t stored = preferences.getUChar(key, static_cast<uint8_t>(255));
    const bool ok = stored == value;
    if (!ok) {
      logger::errorf("[NVS] reset u8 failed key=%s wrote=%u stored=%u expected=%u",
                     key,
                     static_cast<unsigned>(written),
                     static_cast<unsigned>(stored),
                     static_cast<unsigned>(value));
    }
    return ok;
  };

  auto writeAndVerifyFloat = [&](const char* key, const float value) {
    const size_t written = preferences.putFloat(key, value);
    const float stored = preferences.getFloat(key, -9999.0f);
    const float delta = stored > value ? (stored - value) : (value - stored);
    const bool ok = delta <= 0.05f;
    if (!ok) {
      logger::errorf("[NVS] reset float failed key=%s wrote=%u stored=%.3f expected=%.3f",
                     key,
                     static_cast<unsigned>(written),
                     stored,
                     value);
    }
    return ok;
  };

  writeOk = writeAndVerifyUChar("console_profile", static_cast<uint8_t>(DEFAULT_CONSOLE_PROFILE)) && writeOk;
  writeOk = writeAndVerifyUChar("icon_mode", 1) && writeOk;
  writeOk = writeAndVerifyFloat(kThresholdOffsetKey, 0.0f) && writeOk;

  const ConsoleProfile profiles[] = {
      ConsoleProfile::PS5_FAT,
      ConsoleProfile::PS4_PRO,
      ConsoleProfile::PS3_FAT,
  };

  for (const ConsoleProfile profile : profiles) {
    const ProfileThresholdPreset defaults = getDefaultProfileThresholdPreset(profile);
    const char* prefix = getProfileStoragePrefix(profile);

    char keyIdleCool[16];
    char keyIdleHot[16];
    char keyGameCool[16];
    snprintf(keyIdleCool, sizeof(keyIdleCool), "%s_i_cool", prefix);
    snprintf(keyIdleHot, sizeof(keyIdleHot), "%s_i_hot", prefix);
    snprintf(keyGameCool, sizeof(keyGameCool), "%s_g_cool", prefix);

    writeOk = writeAndVerifyFloat(keyIdleCool, defaults.idleCoolMax) && writeOk;
    writeOk = writeAndVerifyFloat(keyIdleHot, defaults.gameCoolMax) && writeOk;
    writeOk = writeAndVerifyFloat(keyGameCool, defaults.gameHotMax) && writeOk;
  }

  return writeOk;
}

// Tick runtime principal: IO + Web + acquisition capteur + rendu.
void updateApp() {
  updateButtonState();
  wifi_manager::updateWifi();
  getAppState().wifiConnected = wifi_manager::isConnected();
  webServer.handleClient();

  const uint32_t nowMs = millis();
  if (nowMs - lastDisplayMs < DISPLAY_REFRESH_MS) {
    return;
  }
  lastDisplayMs = nowMs;

  auto& state = getAppState();
  const PwmSample sample = pwmSampler.readAndReset();

  if (sample.valid) {
    state.pwmDetected = true;
    lastValidSignalMs = nowMs;

    if (state.isFirstSample) {
      state.dutyFiltered = sample.dutyPercent;
      state.frequencyFiltered = sample.frequencyHz;
      state.isFirstSample = false;
    } else {
      state.dutyFiltered = state.dutyFiltered * FILTER_KEEP_RATIO + sample.dutyPercent * FILTER_NEW_RATIO;
      state.frequencyFiltered = state.frequencyFiltered * FILTER_KEEP_RATIO + sample.frequencyHz * FILTER_NEW_RATIO;
    }
  } else if (nowMs - lastValidSignalMs >= SIGNAL_LOST_MS) {
    state.pwmDetected = false;
    state.dutyFiltered = 0.0f;
    state.frequencyFiltered = 0.0f;
    state.isFirstSample = true;
  }

  if (state.pwmDetected && nowMs - lastGraphMs >= GRAPH_UPDATE_MS) {
    lastGraphMs = nowMs;
    addHistoryPoint(state.dutyFiltered);
  }

  updateLedState();
  drawCurrentPage();
}
