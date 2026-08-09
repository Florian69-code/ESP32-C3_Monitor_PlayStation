#pragma once

#include <Arduino.h>

#include "../app/app_state.h"

namespace web_pages {

// Initialise l'acces au stockage (LittleFS) utilise pour les templates/pages.
// Retourne true si le stockage est pret.
bool initializeStorage();

// Retourne true si le stockage de templates est operationnel.
bool isStorageReady();

// Retourne la feuille CSS fallback integree au firmware.
// Utilisee quand /assets/style.css n'est pas lisible depuis LittleFS.
String getSharedCss();

// Retourne la page login rendue.
// errorMessage peut contenir un message utilisateur a afficher.
String getLoginPage(const String& errorMessage = String());

// Retourne la page d'accueil avec informations runtime et navigation.
String getHomePage(const AppState& state);

// Retourne la page historique (graphe + script fetch API).
String getHistoryPage();

// Retourne la page des températures des sondes (graphe + script fetch API).
String getTemperaturePage();

// Retourne la page profil avec seuils, mode icone et message d'etat optionnel.
String getProfilePage(const AppState& state, const String& statusMessage = String());

// Retourne la page console logs.
// - title: titre de page
// - logContent: logs pre-rendus HTML
// - refreshSeconds: periode d'auto-refresh meta
// - currentLogLevel: niveau de log selectionne
// - statusMessage: message succes/erreur optionnel
String getConsolePage(const String& title,
					  const String& logContent,
					  uint16_t refreshSeconds,
					  uint8_t currentLogLevel,
					  const String& statusMessage = String());

// Retourne une page HTML de redirection immediate vers url.
String getRedirectPage(const char* url);

}  // namespace web_pages
