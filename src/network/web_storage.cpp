#include "web_storage.h"

#include <FS.h>
#include <LittleFS.h>

#include "../utils/logger.h"

namespace {

bool gInitialized = false;
bool gReady = false;

String loadFileToString(const char* path) {
  if (!gReady) {
    return String();
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    logger::error(String("[WEB] template missing: ") + path);
    return String();
  }

  String content;
  content.reserve(static_cast<unsigned int>(file.size()) + 16U);
  while (file.available()) {
    content += static_cast<char>(file.read());
  }
  file.close();
  return content;
}

}  // namespace

bool web_storage::initialize() {
  if (gInitialized) {
    return gReady;
  }

  gInitialized = true;
  gReady = LittleFS.begin(false);

  if (gReady) {
    logger::info("[WEB] LittleFS mounted");
  } else {
    logger::error("[WEB] LittleFS mount failed");
  }

  return gReady;
}

bool web_storage::isReady() {
  return gReady;
}

bool web_storage::sendStaticFile(WebServer& server,
                                 const char* path,
                                 const char* contentType,
                                 const char* cacheControl) {
  if (!gReady) {
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    logger::warn(String("[WEB] static file missing: ") + path);
    return false;
  }

  if (cacheControl != nullptr) {
    server.sendHeader("Cache-Control", cacheControl);
  }

  server.streamFile(file, contentType);
  file.close();
  return true;
}

String web_storage::renderTemplate(const char* path, const TemplateToken* tokens, size_t tokenCount) {
  String html = loadFileToString(path);
  if (html.length() == 0) {
    return String();
  }

  for (size_t i = 0; i < tokenCount; ++i) {
    html.replace(tokens[i].key, tokens[i].value);
  }

  return html;
}
