#include "pwm_sampler.h"

#include <driver/gpio.h>
#include <esp_timer.h>

#include "config.h"

// Variables statiques partagées entre l'interruption et la lecture principale.
// Elles doivent être déclarées volatiles parce que leur valeur peut changer
// en dehors du flot normal d'exécution.
namespace {
volatile uint64_t totalHighUs = 0;   // Somme des durées du signal haut en microsecondes.
volatile uint64_t totalLowUs = 0;    // Somme des durées du signal bas en microsecondes.
volatile uint32_t periods = 0;       // Nombre de fronts montants détectés.
volatile uint64_t lastEdgeUs = 0;    // Horodatage de la dernière transition.
volatile bool lastLevel = false;     // Niveau précédent observé du signal.

uint8_t interruptPin = 0;            // Broche sur laquelle l'interruption est attachée.

// Interruption appelée sur chaque changement de niveau du signal PWM.
// Elle mesure la durée entre deux transitions et met à jour les compteurs.
void IRAM_ATTR handlePwmChange() {
  const uint64_t nowUs = esp_timer_get_time();
  const bool level = gpio_get_level(static_cast<gpio_num_t>(interruptPin));
  const uint64_t deltaUs = nowUs - lastEdgeUs;

  lastEdgeUs = nowUs;

  if (lastLevel) {
    totalHighUs += deltaUs;
  } else {
    totalLowUs += deltaUs;
  }

  // Conserver le niveau courant pour la prochaine transition.
  lastLevel = level;

  // Un front montant marque la fin d'une période complète du signal PWM.
  if (level) {
    periods++;
  }
}
}

PwmSampler::PwmSampler(uint8_t inputPin) : pin(inputPin) {}

void PwmSampler::begin() {
  interruptPin = pin;
  pinMode(pin, INPUT);

  // Lire l'état initial du signal et démarrer la mesure à partir de maintenant.
  lastLevel = digitalRead(pin);
  lastEdgeUs = esp_timer_get_time();

  attachInterrupt(digitalPinToInterrupt(pin), handlePwmChange, CHANGE);
}

PwmSample PwmSampler::readAndReset() {
  noInterrupts();

  const uint64_t highUs = totalHighUs;
  const uint64_t lowUs = totalLowUs;
  const uint32_t periodCount = periods;

  // Remise à zéro atomique des compteurs pour la prochaine période de mesure.
  totalHighUs = 0;
  totalLowUs = 0;
  periods = 0;

  interrupts();

  PwmSample sample;
  const uint64_t totalUs = highUs + lowUs;

  // Validation minimale : nombre de périodes et présence de signal.
  if (periodCount < MIN_PERIODS_PER_SAMPLE || totalUs == 0) {
    return sample;
  }

  // Éviter division par zéro ou valeurs invalides pour la fréquence
  if (totalUs < 1000) {
    return sample;
  }

  sample.valid = true;
  sample.dutyPercent = (100.0f * static_cast<float>(highUs)) / static_cast<float>(totalUs);
  sample.frequencyHz = static_cast<float>(periodCount) / (static_cast<float>(totalUs) / 1000000.0f);
  return sample;
}
