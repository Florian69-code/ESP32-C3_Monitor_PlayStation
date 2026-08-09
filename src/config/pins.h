#pragma once

#include <Arduino.h>

// === Hardware pin mapping ===
constexpr uint8_t PWM_INPUT_PIN = 2; // GPIO de lecture du signal PWM ventilateur.
constexpr uint8_t OLED_SDA_PIN = 5; // GPIO SDA pour le bus I2C de l'OLED.
constexpr uint8_t OLED_SCL_PIN = 6; // GPIO SCL pour le bus I2C de l'OLED.
constexpr uint8_t BUTTON_PIN = 9; // GPIO du bouton utilisateur.
constexpr uint8_t LED_PIN = 8; // GPIO de la LED d'indication thermique.

// === Comportement matériel ===
constexpr bool BUTTON_ACTIVE_LOW = true; // true si le bouton tire la ligne a 0 lorsqu'il est appuye.
constexpr bool LED_TEMP_ENABLED = true; // Active l'utilisation de la LED de temperature.
