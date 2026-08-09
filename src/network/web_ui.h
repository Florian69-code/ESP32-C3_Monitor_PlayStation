#pragma once

#include <Arduino.h>
#include <WebServer.h>

namespace web_ui {

// Enregistre toutes les routes HTTP de l'interface Web:
// login, pages applicatives, API JSON, actions profil/console.
// Cette fonction ne lance pas server.begin(); elle prepare seulement les handlers.
void initializeRoutes(WebServer& server);

}  // namespace web_ui
