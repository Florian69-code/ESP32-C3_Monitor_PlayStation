#pragma once

#include <Arduino.h>

#include "../config/log_config.h"

namespace logger {

// Niveaux de severite utilisables par le logger.
enum class LogLevel : uint8_t {
  TRACE = static_cast<uint8_t>(log_config::LogLevel::TRACE),
  DEBUG = static_cast<uint8_t>(log_config::LogLevel::DEBUG),
  INFO = static_cast<uint8_t>(log_config::LogLevel::INFO),
  WARN = static_cast<uint8_t>(log_config::LogLevel::WARN),
  ERROR = static_cast<uint8_t>(log_config::LogLevel::ERROR),
};

// Initialise le logger (reset buffer + niveau par defaut).
void initialize();

// Vide le buffer circulaire des logs en memoire.
void clear();

// Definit le niveau minimum a enregistrer/afficher.
void setLevel(LogLevel level);

// Retourne le niveau minimum actuel.
LogLevel getLevel();

// Retourne le nom texte associe a un niveau (TRACE/DEBUG/INFO/WARN/ERROR).
const char* getLevelName(LogLevel level);

// Enregistre une ligne de log au niveau choisi.
void log(LogLevel level, const String& message);

// Raccourcis de log par niveau.
void trace(const String& message);
void debug(const String& message);
void info(const String& message);
void warn(const String& message);
void error(const String& message);

// Variantes printf-formattee. Taille du buffer interne limitee.
void tracef(const char* format, ...);
void debugf(const char* format, ...);
void infof(const char* format, ...);
void warnf(const char* format, ...);
void errorf(const char* format, ...);

// Retourne le contenu du buffer de logs pre-rendu en HTML.
// Utilise par la page Web /console.
String getHtmlLogContent();

}  // namespace logger
