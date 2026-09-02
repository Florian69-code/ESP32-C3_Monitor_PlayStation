#include "wifi_manager.h"

#include <WiFi.h>

#include "../config/web_config.h"
#include "../utils/logger.h"
#include "web_ui.h"

namespace {

bool gWifiConnected = false;
uint32_t gConnectionStartMs = 0;
const char* gConnectionStatusText = "DISCONNECTED";

}  // namespace

bool wifi_manager::initializeWifi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.persistent(false);
  WiFi.setHostname("ps5fan");

  IPAddress localIp(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  gConnectionStartMs = millis();
  gConnectionStatusText = "AP_STARTING";
  logger::info("[WiFi] starting access point");
  logger::infof("[WiFi] SSID: %s", web_config::WIFI_SSID);

  const bool configOk = WiFi.softAPConfig(localIp, gateway, subnet);
  const bool apStarted = WiFi.softAP(web_config::WIFI_SSID, web_config::WIFI_PASSWORD);
  gWifiConnected = configOk && apStarted;

  if (gWifiConnected) {
    gConnectionStatusText = "AP_READY";
    logger::info("[WiFi] access point ready");
    logger::debugf("[WiFi] IP: %s", WiFi.softAPIP().toString().c_str());
  } else {
    gConnectionStatusText = "AP_FAILED";
    logger::error("[WiFi] access point setup failed");
    logger::debugf("[WiFi] softAPConfig=%s", configOk ? "true" : "false");
    logger::debugf("[WiFi] softAP=%s", apStarted ? "true" : "false");
    logger::errorf("[WiFi] connection attempt started at ms=%lu", static_cast<unsigned long>(gConnectionStartMs));
  }

  return gWifiConnected;
}

void wifi_manager::updateWifi() {
  if (!gWifiConnected) {
    return;
  }

  // Pas de log trace pour ne pas polluer les logs
  // const uint8_t stationCount = WiFi.softAPgetStationNum();
  // Debug uniquement si besoin : logger::debugf("[WiFi] station connected, count=%d", stationCount);
}

bool wifi_manager::isConnected() {
  return gWifiConnected;
}

String wifi_manager::getIpAddress() {
  return gWifiConnected ? WiFi.softAPIP().toString() : String("0.0.0.0");
}

const char* wifi_manager::getConnectionStatusText() {
  return gConnectionStatusText;
}

void wifi_manager::initializeWebServer(WebServer& server) {
  web_ui::initializeRoutes(server);
}
