#pragma once

#include <Arduino.h>

namespace log_config {

enum class LogLevel : uint8_t {
  TRACE = 0,
  DEBUG = 1,
  INFO = 2,
  WARN = 3,
  ERROR = 4,
};

// Niveau minimum des messages journalises (TRACE..ERROR).
constexpr LogLevel CURRENT_LOG_LEVEL = LogLevel::DEBUG;
// Nombre de lignes conservees en memoire pour l'historique des logs.
constexpr uint8_t LOG_RING_BUFFER_LINES = 128;
// Intervalle de rafraichissement de la page web des logs (en secondes).
constexpr uint16_t LOG_WEB_REFRESH_SECONDS = 5;

}  // namespace log_config
