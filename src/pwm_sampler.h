#pragma once

#include <Arduino.h>

// Structure representant un echantillon PWM calcule sur une fenetre de mesure.
// valid: true si l'echantillon respecte les criteres minimaux de validite.
// dutyPercent: rapport cyclique en pourcentage [0..100].
// frequencyHz: frequence estimee en hertz.
struct PwmSample {
  bool valid = false;
  float dutyPercent = 0.0f;
  float frequencyHz = 0.0f;
};

// Mesureur PWM base interruption.
// Principe:
// - une ISR cumule les durees niveau haut/bas,
// - readAndReset() calcule duty/frequence puis remet les compteurs a zero.
class PwmSampler {
public:
  // Construit un mesureur associe a une broche d'entree PWM.
  explicit PwmSampler(uint8_t inputPin);

  // Configure la broche et active l'interruption CHANGE.
  // Doit etre appelee une fois pendant setup.
  void begin();

  // Retourne l'echantillon courant et reset les accumulateurs.
  // Si les donnees sont insuffisantes, sample.valid reste false.
  PwmSample readAndReset();

private:
  uint8_t pin; // Broche physique du signal PWM.
};

