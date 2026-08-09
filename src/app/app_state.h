#pragma once

#include <Arduino.h>

#include "../config.h"

// Etat centralisé de l'application : l'affichage et la logique métier ne doivent
// plus dépendre de variables globales dispersées dans le sketch.
enum class DisplayPage : uint8_t {
  SimplePwm = 0,
  StatusFace,
  Graph,
  Details,
  Profile,
  Count
};

// Seuils associes a un profil console.
// Chaque seuil est exprime en pourcentage PWM.
struct ProfileThresholds {
  float idleCoolMax = 0.0f;
  float gameCoolMax = 0.0f;
  float gameHotMax = 0.0f;
};

// Etat global partage entre logique applicative, rendu OLED et interface Web.
// Cet objet centralise les valeurs runtime pour limiter la duplication d'etat.
struct AppState {
  // Page OLED courante.
  DisplayPage currentPage = DisplayPage::SimplePwm;
  // Profil console actif (PS3/PS4/PS5).
  ConsoleProfile activeProfile = DEFAULT_CONSOLE_PROFILE;

  // Mesures filtrees en temps reel.
  float dutyFiltered = 0.0f;
  float frequencyFiltered = 0.0f;
  // Valeur max historique observée depuis reset ou changement profil.
  float dutyMax = 0.0f;
  // Offset global applique dynamiquement aux seuils du profil actif.
  float thresholdOffset = 0.0f;

  // Etats de fonctionnement.
  bool pwmDetected = false;
  bool isFirstSample = true;
  bool signalLost = false;
  bool wifiConnected = false;

  // Historique PWM pour graphe OLED/Web.
  float dutyHistory[GRAPH_POINTS] = {};
  // Seuils par profil console.
  ProfileThresholds profileThresholds[CONSOLE_PROFILE_COUNT] = {};

  // Parametres d'affichage et de logs exposes aussi au Web.
  uint8_t iconMode = 1;
  uint8_t webLogLevel = static_cast<uint8_t>(log_config::CURRENT_LOG_LEVEL);
  uint16_t webLogRefreshSeconds = log_config::LOG_WEB_REFRESH_SECONDS;
};

// Retourne la reference vers l'instance unique de l'etat applicatif.
// Le cycle de vie est statique (singleton local au module).
AppState& getAppState();

// Reinitialise le buffer d'historique PWM.
// Place les points a -1.0f (valeur sentinelle "non disponible")
// et remet dutyMax a 0.
void resetHistoryBuffer();
