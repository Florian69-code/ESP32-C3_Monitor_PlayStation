#pragma once

#include <Arduino.h>

// Statistiques runtime en RAM: tendance courte (page Live) et resume de session de jeu.
namespace session_stats {

// Valeur sentinelle des points d'historique non disponibles (dixiemes d'unite).
constexpr int16_t kInvalid = INT16_MIN;

struct Summary {
  bool valid = false;
  bool active = false;
  uint32_t durationS = 0;
  float avgDuty = 0.0f;
  float maxDuty = 0.0f;
  float maxTemp[2] = {NAN, NAN};
  uint32_t hotSeconds = 0;
};

// A appeler a chaque tick d'acquisition (apres mise a jour PWM et sondes).
void update(uint32_t nowMs);

// Session en cours (valid=false si aucune) et derniere session terminee.
Summary currentSession();
Summary lastSession();

// Nombre de points disponibles dans la tendance (pas de LIVE_SAMPLE_S secondes).
uint16_t trendCount();
// Lecture par index chronologique (0 = plus ancien). Valeurs en dixiemes, kInvalid si absent.
int16_t trendDuty(uint16_t index);
int16_t trendTemp(uint8_t sensor, uint16_t index);

// Direction de tendance par rapport a ~30 s avant: +1 hausse, -1 baisse, 0 stable ou inconnu.
// series: 0 = PWM, 1 = sonde 1, 2 = sonde 2.
int8_t trendDirection(uint8_t series, float current);

}  // namespace session_stats
