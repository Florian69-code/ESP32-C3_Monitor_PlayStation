#pragma once

#include <Arduino.h>
#include <DallasTemperature.h>

// === Profil console ===
enum class ConsoleProfile : uint8_t {
  PS5_FAT = 0,
  PS4_PRO = 1,
  PS3_FAT = 2,
  // Les valeurs sont persistees en NVS: ne jamais renumeroter, ajouter a la fin.
  PS5_SLIM = 3,
  PS4_FAT = 4,
  PS3_SLIM = 5
};

// Nombre total de profils de console proposes.
constexpr uint8_t CONSOLE_PROFILE_COUNT = 6;
// Profil charge par defaut a la premiere initialisation.
constexpr ConsoleProfile DEFAULT_CONSOLE_PROFILE = ConsoleProfile::PS5_FAT;

struct ProfileThresholdPreset {
  // Fin de zone IDLE COOL.
  float idleCoolMax;
  // Fin de zone GAME COOL.
  float gameCoolMax;
  // Fin de zone GAME HOT.
  float gameHotMax;
  // Seuil de temperature au repos.
  float temperatureIdle;
  // Seuil de temperature maximale.
  float temperatureMax;
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

// Valeurs de depart pour les variantes Slim/Fat: a calibrer via l'interface Web.
constexpr float STATUS_PS5S_IDLE_COOL_MAX = 12.0f;
constexpr float STATUS_PS5S_GAME_COOL_MAX = 19.0f;
constexpr float STATUS_PS5S_GAME_HOT_MAX = 20.0f;

constexpr float STATUS_PS4F_IDLE_COOL_MAX = 18.0f;
constexpr float STATUS_PS4F_GAME_COOL_MAX = 24.0f;
constexpr float STATUS_PS4F_GAME_HOT_MAX = 25.0f;

constexpr float STATUS_PS3S_IDLE_COOL_MAX = 11.0f;
constexpr float STATUS_PS3S_GAME_COOL_MAX = 17.0f;
constexpr float STATUS_PS3S_GAME_HOT_MAX = 19.0f;

// === Seuils de temperature ===
constexpr float STATUS_PS5F_TEMPERATURE_IDLE = 50.0f;
constexpr float STATUS_PS5F_TEMPERATURE_MAX = 90.0f;

constexpr float STATUS_PS4P_TEMPERATURE_IDLE = 50.0f;
constexpr float STATUS_PS4P_TEMPERATURE_MAX = 90.0f;

constexpr float STATUS_PS3F_TEMPERATURE_IDLE = 50.0f;
constexpr float STATUS_PS3F_TEMPERATURE_MAX = 90.0f;

constexpr float STATUS_PS5S_TEMPERATURE_IDLE = 50.0f;
constexpr float STATUS_PS5S_TEMPERATURE_MAX = 90.0f;

constexpr float STATUS_PS4F_TEMPERATURE_IDLE = 50.0f;
constexpr float STATUS_PS4F_TEMPERATURE_MAX = 90.0f;

constexpr float STATUS_PS3S_TEMPERATURE_IDLE = 50.0f;
constexpr float STATUS_PS3S_TEMPERATURE_MAX = 90.0f;

// === Identifiants et noms des sondes de temperature ===
extern DeviceAddress SONDE1_ID;
constexpr char SONDE1_NAME[] = "";
extern DeviceAddress SONDE2_ID;
constexpr char SONDE2_NAME[] = "";


// === Descriptions des seuils pour l'interface Web ===
constexpr const char WEB_IDLE_COOL_MAX_DESCRIPTION[] = "Seuil max de la zone IDLE: en dessous, le ventilateur est considere au repos.";
constexpr const char WEB_GAME_COOL_MAX_DESCRIPTION[] = "Seuil intermediaire de zone GAME: au-dessus, la chauffe devient soutenue.";
constexpr const char WEB_GAME_HOT_MAX_DESCRIPTION[] = "Seuil max de la zone GAME COOL: au-dessus, l'etat passe en GAME HOT.";
constexpr const char WEB_TEMPERATURE_IDLE_DESCRIPTION[] = "Seuil de temperature au repos: en dessous, le systeme est considere normal.";
constexpr const char WEB_TEMPERATURE_MAX_DESCRIPTION[] = "Seuil de temperature maximale: au-dessus, le systeme est considere en surchauffe.";

inline ProfileThresholdPreset getDefaultProfileThresholdPreset(ConsoleProfile profile) {
  switch (profile) {
    case ConsoleProfile::PS4_PRO:
      return {STATUS_PS4P_IDLE_COOL_MAX, STATUS_PS4P_GAME_COOL_MAX, STATUS_PS4P_GAME_HOT_MAX, STATUS_PS4P_TEMPERATURE_IDLE, STATUS_PS4P_TEMPERATURE_MAX};
    case ConsoleProfile::PS3_FAT:
      return {STATUS_PS3F_IDLE_COOL_MAX, STATUS_PS3F_GAME_COOL_MAX, STATUS_PS3F_GAME_HOT_MAX, STATUS_PS3F_TEMPERATURE_IDLE, STATUS_PS3F_TEMPERATURE_MAX};
    case ConsoleProfile::PS5_SLIM:
      return {STATUS_PS5S_IDLE_COOL_MAX, STATUS_PS5S_GAME_COOL_MAX, STATUS_PS5S_GAME_HOT_MAX, STATUS_PS5S_TEMPERATURE_IDLE, STATUS_PS5S_TEMPERATURE_MAX};
    case ConsoleProfile::PS4_FAT:
      return {STATUS_PS4F_IDLE_COOL_MAX, STATUS_PS4F_GAME_COOL_MAX, STATUS_PS4F_GAME_HOT_MAX, STATUS_PS4F_TEMPERATURE_IDLE, STATUS_PS4F_TEMPERATURE_MAX};
    case ConsoleProfile::PS3_SLIM:
      return {STATUS_PS3S_IDLE_COOL_MAX, STATUS_PS3S_GAME_COOL_MAX, STATUS_PS3S_GAME_HOT_MAX, STATUS_PS3S_TEMPERATURE_IDLE, STATUS_PS3S_TEMPERATURE_MAX};
    case ConsoleProfile::PS5_FAT:
    default:
      return {STATUS_PS5F_IDLE_COOL_MAX, STATUS_PS5F_GAME_COOL_MAX, STATUS_PS5F_GAME_HOT_MAX, STATUS_PS5F_TEMPERATURE_IDLE, STATUS_PS5F_TEMPERATURE_MAX};
  }
}

// Nom affichable du profil (OLED et Web).
inline const char* getConsoleProfileName(ConsoleProfile profile) {
  switch (profile) {
    case ConsoleProfile::PS4_PRO: return "PS4 PRO";
    case ConsoleProfile::PS3_FAT: return "PS3 FAT";
    case ConsoleProfile::PS5_SLIM: return "PS5 SLIM";
    case ConsoleProfile::PS4_FAT: return "PS4 FAT";
    case ConsoleProfile::PS3_SLIM: return "PS3 SLIM";
    case ConsoleProfile::PS5_FAT:
    default: return "PS5 FAT";
  }
}

// Prefixe de cle NVS (3 caracteres max: les cles NVS sont limitees a 15 caracteres).
inline const char* getConsoleProfileStoragePrefix(ConsoleProfile profile) {
  switch (profile) {
    case ConsoleProfile::PS4_PRO: return "p4";
    case ConsoleProfile::PS3_FAT: return "p3";
    case ConsoleProfile::PS5_SLIM: return "p5s";
    case ConsoleProfile::PS4_FAT: return "p4f";
    case ConsoleProfile::PS3_SLIM: return "p3s";
    case ConsoleProfile::PS5_FAT:
    default: return "p5";
  }
}

// Convertit une valeur brute (NVS ou formulaire) en profil; false si inconnue.
inline bool parseConsoleProfileValue(uint8_t raw, ConsoleProfile& profile) {
  if (raw >= CONSOLE_PROFILE_COUNT) {
    return false;
  }
  profile = static_cast<ConsoleProfile>(raw);
  return true;
}

// === Réglages de calibration ===
constexpr float THRESHOLD_STEP = 1.0f; // Pas de reglage lors des ajustements utilisateur.
constexpr float THRESHOLD_MIN = 0.0f; // Borne basse autorisee pour un seuil (%).
constexpr float THRESHOLD_MAX = 100.0f; // Borne haute autorisee pour un seuil (%).
constexpr float TEMPERATURE_MIN = 0.0f; // Borne basse autorisee pour un seuil de temperature (°C).
constexpr float TEMPERATURE_MAX = 120.0f; // Borne haute autorisee pour un seuil de temperature (°C).
constexpr float TEMPERATURE_STEP = 1.0f; // Pas de reglage lors des ajustements utilisateur.
constexpr int TEMPERATURE_REFRESH = 3; // Refresh de la sonde de temperature toutes les N secondes pour eviter que la sonde chauffe trop et que la valeur soit altérée.