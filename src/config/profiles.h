#pragma once

#include <Arduino.h>

// === Profil console ===
enum class ConsoleProfile : uint8_t {
  PS5_FAT = 0,
  PS4_PRO = 1,
  PS3_FAT = 2
};

// Nombre total de profils de console proposes.
constexpr uint8_t CONSOLE_PROFILE_COUNT = 3;
// Profil charge par defaut a la premiere initialisation.
constexpr ConsoleProfile DEFAULT_CONSOLE_PROFILE = ConsoleProfile::PS5_FAT;

struct ProfileThresholdPreset {
  // Fin de zone IDLE COOL.
  float idleCoolMax;
  // Fin de zone GAME COOL.
  float gameCoolMax;
  // Fin de zone GAME HOT.
  float gameHotMax;
};

// === Seuils de statut de la vitesse du ventilateur ===
// Ces seuils sont utilises pour determiner l'etat de chauffe du systeme et afficher
// un visage ou un texte sur l'ecran OLED. Ils sont definis par profil de console et 
// peuvent etre ajustes par l'utilisateur via l'interface Web.
// Le comportement de la PS5 FAT est qu'une fois le jeux arrêté, le ventilateur redescend rapidement a une vitesse faible.
// Le comportement de la PS4 PRO est qu'une fois le jeux arrêté, le ventilateur redescend lentement a une vitesse faible.
// - Si le PWM est en dessous de 12%, l'etat est considere comme "IDLE COOL" (repos).
// - Si le PWM est entre 12% et 19%, l'etat est considere comme "GAME COOL" (chauffe sensible).
// - Si le PWM est au-dessus de 19%, l'etat est considere comme "GAME HOT" (chauffe importante).
constexpr float STATUS_PS5F_IDLE_COOL_MAX = 12.0f; // Seuil max (%) de la zone IDLE COOL pour PS5 FAT.
constexpr float STATUS_PS5F_GAME_COOL_MAX = 19.0f; // Seuil max (%) de la zone GAME COOL pour PS5 FAT.
constexpr float STATUS_PS5F_GAME_HOT_MAX = 20.0f; // Seuil max (%) de la zone GAME HOT pour PS5 FAT.

constexpr float STATUS_PS4P_IDLE_COOL_MAX = 18.0f; // Seuil max (%) de la zone IDLE COOL pour PS4 PRO.
constexpr float STATUS_PS4P_GAME_COOL_MAX = 24.0f; // Seuil max (%) de la zone GAME COOL pour PS4 PRO.
constexpr float STATUS_PS4P_GAME_HOT_MAX = 25.0f; // Seuil max (%) de la zone GAME HOT pour PS4 PRO.

constexpr float STATUS_PS3F_IDLE_COOL_MAX = 11.0f; // Seuil max (%) de la zone IDLE COOL pour PS3 FAT.
constexpr float STATUS_PS3F_GAME_COOL_MAX = 17.0f; // Seuil max (%) de la zone GAME COOL pour PS3 FAT.
constexpr float STATUS_PS3F_GAME_HOT_MAX = 19.0f; // Seuil max (%) de la zone GAME HOT pour PS3 FAT.

constexpr const char WEB_IDLE_COOL_MAX_DESCRIPTION[] =
    "Seuil max de la zone IDLE: en dessous, le ventilateur est considere au repos.";
constexpr const char WEB_GAME_COOL_MAX_DESCRIPTION[] =
    "Seuil intermediaire de zone GAME: au-dessus, la chauffe devient soutenue.";
constexpr const char WEB_GAME_HOT_MAX_DESCRIPTION[] =
    "Seuil max de la zone GAME COOL: au-dessus, l'etat passe en GAME HOT.";

inline ProfileThresholdPreset getDefaultProfileThresholdPreset(ConsoleProfile profile) {
  switch (profile) {
    case ConsoleProfile::PS4_PRO:
      return {STATUS_PS4P_IDLE_COOL_MAX, STATUS_PS4P_GAME_COOL_MAX, STATUS_PS4P_GAME_HOT_MAX};
    case ConsoleProfile::PS3_FAT:
      return {STATUS_PS3F_IDLE_COOL_MAX, STATUS_PS3F_GAME_COOL_MAX, STATUS_PS3F_GAME_HOT_MAX};
    case ConsoleProfile::PS5_FAT:
    default:
      return {STATUS_PS5F_IDLE_COOL_MAX, STATUS_PS5F_GAME_COOL_MAX, STATUS_PS5F_GAME_HOT_MAX};
  }
}

// === Réglages de calibration ===
constexpr float THRESHOLD_STEP = 1.0f; // Pas de reglage lors des ajustements utilisateur.
constexpr float THRESHOLD_MIN = 0.0f; // Borne basse autorisee pour un seuil (%).
constexpr float THRESHOLD_MAX = 100.0f; // Borne haute autorisee pour un seuil (%).
