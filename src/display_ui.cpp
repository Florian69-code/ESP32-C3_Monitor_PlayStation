#include "display_ui.h"

#include <stdio.h>

#include "config/log_config.h"
#include "utils/logger.h"

namespace {
void drawCoolFace(const uint8_t centerX, const uint8_t centerY) {
  display.drawCircle(centerX, centerY, 11);
  display.drawDisc(centerX - 4, centerY - 3, 1);
  display.drawDisc(centerX + 4, centerY - 3, 1);
  display.drawLine(centerX - 5, centerY + 4, centerX - 2, centerY + 7);
  display.drawLine(centerX - 2, centerY + 7, centerX + 2, centerY + 7);
  display.drawLine(centerX + 2, centerY + 7, centerX + 5, centerY + 4);
}

void drawHotFace(const uint8_t centerX, const uint8_t centerY) {
  display.drawCircle(centerX, centerY, 11);
  display.drawLine(centerX - 6, centerY - 5, centerX - 2, centerY - 2);
  display.drawLine(centerX + 6, centerY - 5, centerX + 2, centerY - 2);
  display.drawCircle(centerX, centerY + 5, 3);
  display.drawLine(centerX + 10, centerY - 10, centerX + 14, centerY - 14);
  display.drawLine(centerX + 14, centerY - 14, centerX + 17, centerY - 10);
}

void drawNeutralFace(const uint8_t centerX, const uint8_t centerY) {
  display.drawCircle(centerX, centerY, 11);
  display.drawDisc(centerX - 4, centerY - 3, 1);
  display.drawDisc(centerX + 4, centerY - 3, 1);
  display.drawLine(centerX - 5, centerY + 5, centerX + 5, centerY + 5);
}

void drawGraph() {
  auto& state = getAppState();
  constexpr int graphTop = 13;
  constexpr int graphBottom = 39;
  constexpr int graphHeight = graphBottom - graphTop;
  constexpr int labelWidth = 12;
  constexpr int graphLeft = labelWidth;
  constexpr int graphWidth = SCREEN_WIDTH - graphLeft;

  float minValue = 100.0f;
  float maxValue = 0.0f;
  bool hasValue = false;

  for (uint8_t i = 0; i < GRAPH_VISIBLE_POINTS; i++) {
    const uint8_t historyIndex = GRAPH_POINTS - GRAPH_VISIBLE_POINTS + i;
    const float value = state.dutyHistory[historyIndex];

    if (value < 0.0f) {
      continue;
    }

    hasValue = true;
    minValue = min(minValue, value);
    maxValue = max(maxValue, value);
  }

  display.setFont(u8g2_font_4x6_tf);
  if (!hasValue) {
    return;
  }

  minValue -= GRAPH_MARGIN_PERCENT;
  maxValue += GRAPH_MARGIN_PERCENT;

  if (maxValue - minValue < GRAPH_MIN_SPAN_PERCENT) {
    const float center = (minValue + maxValue) / 2.0f;
    minValue = center - GRAPH_MIN_SPAN_PERCENT / 2.0f;
    maxValue = center + GRAPH_MIN_SPAN_PERCENT / 2.0f;
  }

  minValue = constrain(minValue, 0.0f, 100.0f);
  maxValue = constrain(maxValue, 0.0f, 100.0f);

  if (maxValue - minValue < 1.0f) {
    maxValue = min(100.0f, minValue + GRAPH_MIN_SPAN_PERCENT);
  }

  // Vérification supplémentaire pour éviter division par zéro
  if (maxValue <= minValue) {
    maxValue = minValue + 1.0f;
  }

  char label[8];
  snprintf(label, sizeof(label), "%.0f", maxValue);
  display.drawStr(0, graphTop + 5, label);

  snprintf(label, sizeof(label), "%.0f", minValue);
  display.drawStr(0, graphBottom, label);

  for (uint8_t x = graphLeft; x < SCREEN_WIDTH; x += 6) {
    display.drawPixel(x, graphTop + graphHeight / 2);
  }

  display.drawVLine(graphLeft - 1, graphTop, graphHeight + 1);
  display.drawHLine(graphLeft, graphBottom, graphWidth);

  int previousX = -1;
  int previousY = -1;

  for (uint8_t i = 0; i < GRAPH_VISIBLE_POINTS; i++) {
    const uint8_t historyIndex = GRAPH_POINTS - GRAPH_VISIBLE_POINTS + i;
    const float value = state.dutyHistory[historyIndex];

    if (value < 0.0f) {
      continue;
    }

    const float normalized = (constrain(value, minValue, maxValue) - minValue) / (maxValue - minValue);
    const int x = graphLeft + (i * (graphWidth - 1)) / (GRAPH_VISIBLE_POINTS - 1);
    const int y = graphBottom - static_cast<int>(normalized * graphHeight);

    display.drawPixel(x, y);
    display.drawPixel(x, y - 1);

    if (previousX >= 0) {
      display.drawLine(previousX, previousY, x, y);
    }

    previousX = x;
    previousY = y;
  }
}

const char* getActiveProfileName() {
  switch (getAppState().activeProfile) {
    case ConsoleProfile::PS4_PRO:
      return "PS4 PRO";
    case ConsoleProfile::PS3_FAT:
      return "PS3 FAT";
    case ConsoleProfile::PS5_FAT:
    default:
      return "PS5 FAT";
  }
}

}  // namespace


void drawIconSnowflake(const uint8_t centerX, const uint8_t centerY) {
  // simple flocon en traits
  display.drawCircle(centerX, centerY, 9);
  display.drawLine(centerX, centerY - 9, centerX, centerY + 9);
  display.drawLine(centerX - 9, centerY, centerX + 9, centerY);
  display.drawLine(centerX - 6, centerY - 6, centerX + 6, centerY + 6);
  display.drawLine(centerX - 6, centerY + 6, centerX + 6, centerY - 6);
}

void drawIconWind(const uint8_t centerX, const uint8_t centerY) {
  // traits courbes simulant du vent
  display.drawStr(centerX - 12, centerY - 3, "~~~"); // simple fallback si espace limité
  // Option: remplacer par lignes courbes/traits si voulu
}

void drawIconFlame(const uint8_t cx, const uint8_t cy) {
  display.drawLine(cx, cy - 9, cx - 6, cy + 6);
  display.drawLine(cx - 6, cy + 6, cx + 6, cy + 6);
  display.drawLine(cx + 6, cy + 6, cx, cy - 9);
  display.drawLine(cx, cy - 5, cx - 3, cy + 4);
  display.drawLine(cx - 3, cy + 4, cx + 3, cy + 4);
  display.drawLine(cx + 3, cy + 4, cx, cy - 5);
}

// Centre horizontalement un texte à l'écran OLED.
// Cette fonction simplifie l'affichage des titres et des messages sur la petite résolution de l'écran.
void drawCenteredText(const char* text, const uint8_t y) {
  const uint8_t width = display.getStrWidth(text);
  const uint8_t x = width >= SCREEN_WIDTH ? 0 : (SCREEN_WIDTH - width) / 2;
  display.drawStr(x, y, text);
}

// Affiche l'image du profil actif conformément au démarrage décrit dans l'architecture.
void drawProfileImageScreen() {
  display.clearBuffer();
  display.setFont(u8g2_font_7x13B_tf);

  switch (getAppState().activeProfile) {
    case ConsoleProfile::PS5_FAT:
      drawCenteredText("PS5 FAT", 13);
      break;
    case ConsoleProfile::PS4_PRO:
      drawCenteredText("PS4 PRO", 13);
      break;
    case ConsoleProfile::PS3_FAT:
    default:
      drawCenteredText("PS3 FAT", 13);
      break;
  }

  display.sendBuffer();
}

// Affiche l'écran de démarrage par défaut avant le lancement du traitement PWM.
void drawBootScreen() {
  drawProfileImageScreen();
}

// Maintient l'affichage du profil actif pendant la séquence de démarrage.
void drawProfileBootAnimation(const uint8_t frame) {
  (void)frame;
  drawProfileImageScreen();
}

void drawWifiErrorScreen() {
  static uint32_t lastScrollMs = 0;
  static int16_t scrollX = 0;

  const char* message = "WIFI AP FAILED - CHECK PASSWORD";
  const int16_t messageWidth = display.getStrWidth(message);

  if (millis() - lastScrollMs >= 120) {
    lastScrollMs = millis();
    if (messageWidth > SCREEN_WIDTH) {
      scrollX = (scrollX + 1) % (messageWidth + SCREEN_WIDTH);
    }
  }

  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tf);
  drawCenteredText("WIFI - KO", 8);

  display.setFont(u8g2_font_6x10_tf);
  int16_t x = SCREEN_WIDTH - scrollX;
  while (x < SCREEN_WIDTH) {
    display.drawStr(x, 23, message);
    x += messageWidth;
  }

  display.sendBuffer();
  logger::error("[ERROR] WiFi AP setup failed");
}

// Affiche un message d'erreur si aucun signal PWM valide n'est détecté.
void drawNoPwmMessage() {
  // RG: ne journaliser cette erreur qu'une fois toutes les X secondes, avec X = LOG_WEB_REFRESH_SECONDS.
  static uint32_t lastPwmErrorLogMs = 0;
  static bool hasLoggedPwmError = false;

  static uint32_t lastScrollMs = 0;
  static int16_t scrollX = 0;

  const char* message = "PWM ABSENT - VERIFIER SIGNAL";
  const int16_t messageWidth = display.getStrWidth(message);

  if (millis() - lastScrollMs >= 120) {
    lastScrollMs = millis();
    if (messageWidth > SCREEN_WIDTH) {
      scrollX = (scrollX + 1) % (messageWidth + SCREEN_WIDTH);
    }
  }

  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tf);
  drawCenteredText("PWM - KO", 8);

  display.setFont(u8g2_font_6x10_tf);
  int16_t x = SCREEN_WIDTH - scrollX;
  while (x < SCREEN_WIDTH) {
    display.drawStr(x, 23, message);
    x += messageWidth;
  }

  display.sendBuffer();

  const uint32_t nowMs = millis();
  const uint32_t logIntervalMs = static_cast<uint32_t>(log_config::LOG_WEB_REFRESH_SECONDS) * 1000UL;
  if (!hasLoggedPwmError || (nowMs - lastPwmErrorLogMs) >= logIntervalMs) {
    lastPwmErrorLogMs = nowMs;
    hasLoggedPwmError = true;
    logger::error("[ERROR] PWM signal not detected");
  }
}

// Affiche la valeur PWM filtrée en mode simple, sous forme de pourcentage.
void drawSimplePwm() {
  auto& state = getAppState();
  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tf);
  drawCenteredText("PWM", 7);

  char text[8];
  snprintf(text, sizeof(text), "%.0f%%", state.dutyFiltered);
  display.setFont(u8g2_font_logisoso24_tf);
  if (display.getStrWidth(text) > SCREEN_WIDTH) {
    display.setFont(u8g2_font_logisoso18_tf);
  }
  drawCenteredText(text, 36);

  display.sendBuffer();
}

// Affiche une vue synthétique du statut du ventilateur sous forme de visage.
// L'état froid/chaud est calculé à partir des seuils du profil actif.
void drawStatusFace() {
  auto& state = getAppState();
  display.clearBuffer();

  const float idleCoolMax = getActiveIdleCoolMax();
  const float gameHotMax = getActiveGameHotMax();

  const float d = state.dutyFiltered;

  const bool isIdle = d < idleCoolMax;
  const bool isNormal = d >= idleCoolMax && d < gameHotMax;

  if (state.iconMode == 1) {
    display.setFont(u8g2_font_6x10_tf);
    if (d < idleCoolMax) {
      drawCenteredText("IDLE", 9);
    } else if (d < gameHotMax) {
      drawCenteredText("MID", 9);
    } else {
      drawCenteredText("GAME", 9);
    }

    if (isIdle) {
      drawCoolFace(36, 25);
    } else if (isNormal) {
      drawNeutralFace(36, 25);
    } else {
      drawHotFace(36, 25);
    }
  } else {
    display.setFont(u8g2_font_logisoso18_tf);
    drawCenteredText(isIdle ? "IDEL" : (isNormal ? "OK" : "HOT"), 34);
  }

  display.sendBuffer();
}

// Affiche la vue graphique avec l'historique récent du PWM.
void drawGraphDashboard() {
  auto& state = getAppState();
  display.clearBuffer();
  display.setFont(u8g2_font_4x6_tf);

  char text[20];

  snprintf(text, sizeof(text), "PWM %2.0f%%", state.dutyFiltered);
  display.drawStr(0, 7, text);

  snprintf(text, sizeof(text), "Max:%.0f", state.dutyMax);
  display.drawStr(SCREEN_WIDTH - display.getStrWidth(text), 7, text);

  drawGraph();
  display.sendBuffer();
}

// Affiche les détails bruts : PWM courant, PWM max observé et fréquence.
void drawTemperaturePage() {
  auto& state = getAppState();
  display.clearBuffer();

  display.setFont(u8g2_font_4x6_tf);
  display.drawStr(15, 7, "Temperature");

  const bool hasFirst = state.sensorConfig[0].detected && !isnan(state.sensorConfig[0].currentTemperature);
  const bool hasSecond = state.sensorConfig[1].detected && !isnan(state.sensorConfig[1].currentTemperature);

  char text[12];
  display.setFont(u8g2_font_5x7_tf);
  if (hasFirst) {
    snprintf(text, sizeof(text), "%3.0f", state.sensorConfig[0].currentTemperature);
  } else {
    snprintf(text, sizeof(text), "N/A");
  }
  display.drawStr(2, 22, text);

  display.setFont(u8g2_font_5x7_tf);
  display.drawStr(30, 22, "|");

  display.setFont(u8g2_font_5x7_tf);
  if (hasSecond) {
    snprintf(text, sizeof(text), "%3.0f", state.sensorConfig[1].currentTemperature);
  } else {
    snprintf(text, sizeof(text), "N/A");
  }
  display.drawStr(45, 22, text);

  display.setFont(u8g2_font_4x6_tf);
  display.drawStr(2, 38, state.sensorConfig[0].name.length() > 0 ? state.sensorConfig[0].name.c_str() : "sonde1");
  display.drawStr(45, 38, state.sensorConfig[1].name.length() > 0 ? state.sensorConfig[1].name.c_str() : "sonde2");

  display.sendBuffer();
}

void drawDetails() {
  auto& state = getAppState();
  display.clearBuffer();

  char text[20];
  display.setFont(u8g2_font_5x7_tf);
  display.drawStr(0, 7, "PWM");
  display.drawStr(42, 7, "MAX");

  display.setFont(u8g2_font_7x13B_tf);
  snprintf(text, sizeof(text), "%2.0f", state.dutyFiltered);
  display.drawStr(0, 22, text);
  display.setFont(u8g2_font_5x7_tf);
  display.drawStr(21, 22, "%");

  display.setFont(u8g2_font_7x13B_tf);
  snprintf(text, sizeof(text), "%2.0f", state.dutyMax);
  display.drawStr(42, 22, text);
  display.setFont(u8g2_font_5x7_tf);
  display.drawStr(63, 22, "%");

  display.setFont(u8g2_font_6x10_tf);
  snprintf(text, sizeof(text), "%4.0f Hz", state.frequencyFiltered);
  drawCenteredText(text, 39);

  display.sendBuffer();
}

// Affiche le profil actif comme sur l'ecran de demarrage.
void drawProfilePage() {
  drawProfileImageScreen();
}

// Sélectionne la page d'affichage active selon l'état courant du système.
void drawCurrentPage() {
  auto& state = getAppState();
  if (state.currentPage == DisplayPage::Profile) {
    drawProfilePage();
    return;
  }

  if (state.currentPage == DisplayPage::Temperature) {
    drawTemperaturePage();
    return;
  }

  if (!state.wifiConnected) {
    drawWifiErrorScreen();
    return;
  }

  if (!state.pwmDetected) {
    drawNoPwmMessage();
    return;
  }

  switch (state.currentPage) {
    case DisplayPage::SimplePwm:
      drawSimplePwm();
      break;
    case DisplayPage::StatusFace:
      drawStatusFace();
      break;
    case DisplayPage::Graph:
      drawGraphDashboard();
      break;
    case DisplayPage::Details:
      drawDetails();
      break;
    case DisplayPage::Profile:
      break;
    case DisplayPage::Count:
      break;
  }
}

// Retourne le seuil de ventilation "idle cool" associé au profil actif.
float getActiveIdleCoolMax() {
  const uint8_t idx = static_cast<uint8_t>(getAppState().activeProfile);
  const float base = getAppState().profileThresholds[idx].idleCoolMax;
  return constrain(base + getAppState().thresholdOffset, THRESHOLD_MIN, THRESHOLD_MAX);
}

// Retourne le seuil de ventilation "game cool" associé au profil actif.
float getActiveGameCoolMax() {
  const uint8_t idx = static_cast<uint8_t>(getAppState().activeProfile);
  const float base = getAppState().profileThresholds[idx].gameCoolMax;
  return constrain(base + getAppState().thresholdOffset, THRESHOLD_MIN, THRESHOLD_MAX);
}

// Retourne le seuil de ventilation "game hot" associé au profil actif.
float getActiveGameHotMax() {
  const uint8_t idx = static_cast<uint8_t>(getAppState().activeProfile);
  const float base = getAppState().profileThresholds[idx].gameHotMax;
  return constrain(base + getAppState().thresholdOffset, THRESHOLD_MIN, THRESHOLD_MAX);
}