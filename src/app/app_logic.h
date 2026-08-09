#pragma once

#include <Arduino.h>

#include "app_state.h"

// Orchestration du cœur applicatif : lecture PWM, mise à jour de l'état,
// navigation du bouton et gestion des transitions de page.

// Initialise tout le systeme applicatif.
// Actions principales:
// - configure IO (bouton, LED), logger, preferences,
// - initialise OLED et sequence de boot,
// - demarre WiFi AP et serveur Web,
// - initialise le module de mesure PWM.
void initializeApp();

// Met a jour le systeme a chaque tour de loop.
// Cette fonction gere:
// - lecture bouton,
// - maintenance WiFi/Web,
// - acquisition PWM et filtrage,
// - mise a jour historique,
// - rendu de la page OLED active.
void updateApp();

// Met a jour l'etat du bouton de navigation.
// Detecte appui court vs appui long avec debounce.
void updateButtonState();

// Traite un appui court: passage a la page suivante et sauvegarde NVS.
void handleShortPress();

// Traite un appui long: retour a la page SimplePwm et sauvegarde NVS.
void handleLongPress();

// Met a jour le clignotement LED en fonction des seuils PWM.
// Clignotement lent au-dessus du seuil GAME, rapide au-dessus de GAME + 10.
void updateLedState();

// Sauvegarde les reglages de logs Web en Preferences puis met a jour l'etat runtime.
// Retourne true si la sauvegarde est valide et ecrite, false sinon.
bool saveWebLogSettings(uint8_t logLevel, uint16_t refreshSeconds);

// Reinitialise en NVS tous les reglages de la page Profil:
// - profil console actif
// - mode image OLED
// - seuils IDLE/GAME de tous les profils.
// Retourne true si toutes les ecritures Preferences ont reussi.
bool resetProfilePageSettingsToDefaults();
