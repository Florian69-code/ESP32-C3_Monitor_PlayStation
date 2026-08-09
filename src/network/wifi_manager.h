#pragma once

#include <Arduino.h>
#include <WebServer.h>

namespace wifi_manager {

// Initialise le mode WiFi AP (point d'acces local).
// Retourne true si la configuration IP et le demarrage AP reussissent.
bool initializeWifi();

// Met a jour les informations runtime WiFi.
// Fonction non bloquante appelee dans la loop.
void updateWifi();

// Indique si le point d'acces est operationnel.
bool isConnected();

// Retourne l'adresse IP locale de l'AP ("0.0.0.0" si indisponible).
String getIpAddress();

// Retourne un texte de statut synthétique (AP_STARTING, AP_READY, AP_FAILED...).
const char* getConnectionStatusText();

// Branche les routes HTTP applicatives sur l'instance de WebServer fournie.
void initializeWebServer(WebServer& server);

}  // namespace wifi_manager
