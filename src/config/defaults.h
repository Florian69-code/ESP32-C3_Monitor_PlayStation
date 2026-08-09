#pragma once

#include <Arduino.h>

// === Affichage OLED ===
constexpr uint8_t SCREEN_WIDTH = 72;   // Largeur ecran en pixels.
constexpr uint8_t SCREEN_HEIGHT = 40;  // Hauteur ecran en pixels.
constexpr uint8_t OLED_CONTRAST = 255; // Contraste OLED (0-255).

// === Debug série ===
constexpr bool SERIAL_DEBUG_ENABLED = true; // Active les traces sur le port serie.
constexpr uint32_t SERIAL_BAUDRATE = 115200; // Vitesse de communication serie.

// === Timing et rafraîchissements ===
constexpr uint32_t DISPLAY_REFRESH_MS = 100; // Periode de rafraichissement de l'affichage.
constexpr uint32_t GRAPH_UPDATE_MS = 10000; // Intervalle de mise a jour d'un point d'historique.
constexpr uint32_t BOOT_SCREEN_MS = 5000; // Duree d'affichage de l'ecran de demarrage.
constexpr uint32_t BUTTON_DEBOUNCE_MS = 180; // Filtre anti-rebond pour le bouton.
constexpr uint32_t BUTTON_LONG_PRESS_MS = 800; // Seuil pour considerer un appui long.
constexpr uint32_t SIGNAL_LOST_MS = 800; // Delai sans impulsion avant perte de signal PWM.

// === Graphe d'historique ===
constexpr uint8_t GRAPH_POINTS = 72; // Taille totale du buffer d'historique.
constexpr uint8_t GRAPH_VISIBLE_POINTS = 30; // Nombre de points affiches a l'ecran.
static_assert(GRAPH_POINTS >= 2, "GRAPH_POINTS must be >= 2");
constexpr float GRAPH_MIN_SPAN_PERCENT = 10.0f; // Ecart min vertical du graphe pour rester lisible.
constexpr float GRAPH_MARGIN_PERCENT = 2.0f; // Marge ajoutee au min/max pour eviter un tracage colle aux bords.
constexpr uint8_t MIN_PERIODS_PER_SAMPLE = 10; // Nombre min de periodes PWM pour valider un echantillon.

// === Filtrage des mesures ===
constexpr float FILTER_KEEP_RATIO = 0.90f; // Poids de la mesure precedente (filtre exponentiel).
constexpr float FILTER_NEW_RATIO = 0.10f; // Poids de la nouvelle mesure.
