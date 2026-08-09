#pragma once

#include <Arduino.h>

namespace web_config {

constexpr char WEB_TITLE[] = "Configuration ESP32-C3"; // Titre affiche dans l'interface web.
constexpr char WIFI_SSID[] = "PlastationFan"; // Nom du point d'acces Wi-Fi cree par l'ESP32.
constexpr char WIFI_PASSWORD[] = "Pl@ystati0nf@n"; // Mot de passe du point d'acces Wi-Fi.
constexpr char WEB_HOST_URL[] = "http://192.168.4.1"; // URL locale d'acces a l'interface web.
constexpr uint16_t WEB_PORT = 80; // Port HTTP ecoute par le serveur web embarque.
constexpr char WEB_PASSWORD[] = "S0ny"; // Mot de passe de connexion a l'interface web.
constexpr char WEB_SESSION_COOKIE[] = "ps-fan_auth"; // Nom du cookie d'authentification de session.

}  // namespace web_config
