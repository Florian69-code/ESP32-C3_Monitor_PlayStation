#include "logger.h"

#include <stdarg.h>

namespace {

constexpr uint8_t BUFFER_SIZE = log_config::LOG_RING_BUFFER_LINES;
String gLogBuffer[BUFFER_SIZE];
uint8_t gWriteIndex = 0;
uint8_t gCount = 0;
logger::LogLevel gCurrentLogLevel = static_cast<logger::LogLevel>(log_config::CURRENT_LOG_LEVEL);

const char* levelToString(logger::LogLevel level) {
  switch (level) {
    case logger::LogLevel::TRACE:
      return "TRACE";
    case logger::LogLevel::DEBUG:
      return "DEBUG";
    case logger::LogLevel::INFO:
      return "INFO";
    case logger::LogLevel::WARN:
      return "WARN";
    case logger::LogLevel::ERROR:
      return "ERROR";
    default:
      return "LOG";
  }
}

bool shouldLog(logger::LogLevel level) {
  return static_cast<uint8_t>(level) >= static_cast<uint8_t>(gCurrentLogLevel);
}

bool tryParseLevelFromLine(const String& line, logger::LogLevel& level) {
  if (!line.startsWith("[")) {
    return false;
  }

  const int endBracket = line.indexOf(']');
  if (endBracket <= 1) {
    return false;
  }

  const String levelName = line.substring(1, endBracket);
  if (levelName == "TRACE") {
    level = logger::LogLevel::TRACE;
    return true;
  }
  if (levelName == "DEBUG") {
    level = logger::LogLevel::DEBUG;
    return true;
  }
  if (levelName == "INFO") {
    level = logger::LogLevel::INFO;
    return true;
  }
  if (levelName == "WARN") {
    level = logger::LogLevel::WARN;
    return true;
  }
  if (levelName == "ERROR") {
    level = logger::LogLevel::ERROR;
    return true;
  }

  return false;
}

String sanitizeHtml(const String& text) {
  String escaped = text;
  escaped.replace("&", "&amp;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  return escaped;
}

void appendLogLine(const String& line) {
  gLogBuffer[gWriteIndex] = line;
  gWriteIndex = (gWriteIndex + 1) % BUFFER_SIZE;
  if (gCount < BUFFER_SIZE) {
    ++gCount;
  }
}

String formatMessage(logger::LogLevel level, const String& message) {
  return String("[") + levelToString(level) + "] " + message;
}

void logToSerial(logger::LogLevel level, const String& message) {
  if (!shouldLog(level)) {
    return;
  }

  Serial.println(message.c_str());
}

void logPrintf(logger::LogLevel level, const char* format, va_list args) {
  if (!shouldLog(level)) {
    return;
  }

  char buffer[192];
  vsnprintf(buffer, sizeof(buffer), format, args);
  const String message = formatMessage(level, String(buffer));
  appendLogLine(message);
  logToSerial(level, message);
}

}  // namespace

void logger::initialize() {
  clear();
  gCurrentLogLevel = static_cast<logger::LogLevel>(log_config::CURRENT_LOG_LEVEL);
}

void logger::clear() {
  for (uint8_t i = 0; i < BUFFER_SIZE; ++i) {
    gLogBuffer[i] = String();
  }
  gWriteIndex = 0;
  gCount = 0;
}

void logger::setLevel(const LogLevel level) {
  gCurrentLogLevel = level;
}

logger::LogLevel logger::getLevel() {
  return gCurrentLogLevel;
}

const char* logger::getLevelName(const LogLevel level) {
  return levelToString(level);
}

void logger::log(LogLevel level, const String& message) {
  if (!shouldLog(level)) {
    return;
  }

  const String formattedMessage = formatMessage(level, message);
  appendLogLine(formattedMessage);
  logToSerial(level, formattedMessage);
}

void logger::trace(const String& message) {
  log(LogLevel::TRACE, message);
}

void logger::debug(const String& message) {
  log(LogLevel::DEBUG, message);
}

void logger::info(const String& message) {
  log(LogLevel::INFO, message);
}

void logger::warn(const String& message) {
  log(LogLevel::WARN, message);
}

void logger::error(const String& message) {
  log(LogLevel::ERROR, message);
}

void logger::tracef(const char* format, ...) {
  va_list args;
  va_start(args, format);
  logPrintf(LogLevel::TRACE, format, args);
  va_end(args);
}

void logger::debugf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  logPrintf(LogLevel::DEBUG, format, args);
  va_end(args);
}

void logger::infof(const char* format, ...) {
  va_list args;
  va_start(args, format);
  logPrintf(LogLevel::INFO, format, args);
  va_end(args);
}

void logger::warnf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  logPrintf(LogLevel::WARN, format, args);
  va_end(args);
}

void logger::errorf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  logPrintf(LogLevel::ERROR, format, args);
  va_end(args);
}

String logger::getHtmlLogContent() {
  String content;
  const uint8_t visibleCount = gCount;
  for (uint8_t i = 0; i < visibleCount; ++i) {
    const uint8_t index = (gWriteIndex + BUFFER_SIZE - visibleCount + i) % BUFFER_SIZE;
    const String line = gLogBuffer[index];
    if (line.length() == 0) {
      continue;
    }

    logger::LogLevel lineLevel = logger::LogLevel::INFO;
    if (tryParseLevelFromLine(line, lineLevel) && !shouldLog(lineLevel)) {
      continue;
    }

    content += "<div>" + sanitizeHtml(line) + "</div>";
  }

  if (content.length() == 0) {
    content = "<p>Aucun log pour le moment.</p>";
  }

  return content;
}
