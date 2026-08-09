#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

#include "app/app_state.h"
#include "config.h"

// Interface OLED centralisée pour garder le sketch principal lisible.
// Les fonctions de dessin exposent une API simple à la logique métier.
extern U8G2_SSD1306_72X40_ER_F_HW_I2C display;

// Dessine un texte centre horizontalement a la coordonnee Y donnee.
void drawCenteredText(const char* text, uint8_t y);

// Dessine l'ecran de boot standard.
void drawBootScreen();

// Dessine une frame de l'animation de boot basee sur le profil actif.
// Le parametre frame permet d'etendre l'animation sans changer l'API.
void drawProfileBootAnimation(uint8_t frame);

// Affiche la page image du profil actif.
void drawProfileImageScreen();

// Affiche un ecran d'erreur WiFi (AP indisponible).
void drawWifiErrorScreen();

// Affiche un ecran d'erreur si le signal PWM est absent.
void drawNoPwmMessage();

// Affiche la page PWM simple (pourcentage instantane).
void drawSimplePwm();

// Affiche la page statut (smiley/texte selon mode icone).
void drawStatusFace();

// Affiche la page graphe (historique + max).
void drawGraphDashboard();

// Affiche la page details (PWM courant, max, frequence).
void drawDetails();

// Affiche la page profil OLED.
void drawProfilePage();

// Point d'entree principal du rendu.
// Selectionne automatiquement la page ou l'ecran d'erreur a afficher.
void drawCurrentPage();

// Retourne le seuil IDLE COOL actif (profil + offset, borne min/max).
float getActiveIdleCoolMax();

// Retourne le seuil GAME COOL actif (profil + offset, borne min/max).
float getActiveGameCoolMax();

// Retourne le seuil GAME HOT actif (profil + offset, borne min/max).
float getActiveGameHotMax();

// nouveaux dessins/icônes
// Dessine une icone de type flocon autour d'un point central.
void drawIconSnowflake(const uint8_t centerX, const uint8_t centerY);
// Dessine une icone de type vent autour d'un point central.
void drawIconWind(const uint8_t centerX, const uint8_t centerY);
// Dessine une icone de type flamme autour d'un point central.
void drawIconFlame(const uint8_t centerX, const uint8_t centerY);