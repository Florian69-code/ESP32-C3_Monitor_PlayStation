#include "web_pages.h"

#include "../config/log_config.h"
#include "../config/profiles.h"
#include "../config/web_config.h"
#include "web_storage.h"

namespace {

constexpr const char* kPageLogin = "/pages/login.html";
constexpr const char* kPageHome = "/pages/home.html";
constexpr const char* kPageHistory = "/pages/history.html";
constexpr const char* kPageProfile = "/pages/profile.html";
constexpr const char* kPageConsole = "/pages/console.html";
constexpr const char* kPageRedirect = "/pages/redirect.html";
constexpr const char* kPageTemperature = "/pages/temperature.html";


String profileToOption(ConsoleProfile profile, ConsoleProfile activeProfile, const char* label);
String iconModeToOption(uint8_t mode, uint8_t activeMode, const char* label);
String logLevelToOption(uint8_t value, uint8_t selectedValue, const char* label);
String refreshToOption(uint16_t value, uint16_t selectedValue);
String formatThresholdValue(float thresholdOffset);

String buildProfileThresholdsJson(const AppState& state) {
  String json;
  json.reserve(192);
  json += "{";

  for (uint8_t i = 0; i < CONSOLE_PROFILE_COUNT; ++i) {
    if (i > 0) {
      json += ",";
    }

    json += "\"";
    json += String(i);
    json += "\":{";
    json += "\"idle\":";
    json += formatThresholdValue(state.profileThresholds[i].idleCoolMax);
    json += ",\"cool\":";
    json += formatThresholdValue(state.profileThresholds[i].gameCoolMax);
    json += ",\"hot\":";
    json += formatThresholdValue(state.profileThresholds[i].gameHotMax);
    json += "}";
  }

  json += "}";
  return json;
}

const char* getProfileName(ConsoleProfile profile) {
  switch (profile) {
    case ConsoleProfile::PS4_PRO:
      return "PS4 PRO";
    case ConsoleProfile::PS3_FAT:
      return "PS3 FAT";
    case ConsoleProfile::PS5_FAT:
    default:
      return "PS5 FAT";
  }
}

const char* getCurrentLogLevelName(const uint8_t levelValue) {
  switch (levelValue) {
    case static_cast<uint8_t>(log_config::LogLevel::TRACE):
      return "TRACE";
    case static_cast<uint8_t>(log_config::LogLevel::DEBUG):
      return "DEBUG";
    case static_cast<uint8_t>(log_config::LogLevel::INFO):
      return "INFO";
    case static_cast<uint8_t>(log_config::LogLevel::WARN):
      return "WARN";
    case static_cast<uint8_t>(log_config::LogLevel::ERROR):
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

String getNavHtml(const char* activeHome,
                  const char* activeHistory,
                  const char* activeTemperature,
                  const char* activeProfile,
                  const char* activeConsole) {
  String nav;
  nav.reserve(256);
  nav += "<nav>";
  nav += String("<a class=\"") + activeHome + "\" href=\"/\">Accueil</a>";
  nav += String("<a class=\"") + activeHistory + "\" href=\"/history\">Historique PWM</a>";
  nav += String("<a class=\"") + activeTemperature + "\" href=\"/temperature\">Temperature</a>";
  nav += String("<a class=\"") + activeProfile + "\" href=\"/profile\">Profil</a>";
  nav += String("<a class=\"") + activeConsole + "\" href=\"/console\">Console</a>";
  nav += "</nav>";
  return nav;
}

String wrapFallbackPage(const String& title,
                        const String& bodyContent,
                        bool showNav,
                        const char* activeHome,
                        const char* activeHistory,
                        const char* activeTemperature,
                        const char* activeProfile,
                        const char* activeConsole) {
  String html;
  html.reserve(2048 + bodyContent.length());
  html += "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>";
  html += title;
  html += "</title><link rel=\"stylesheet\" href=\"/assets/style.css\"></head><body><div class=\"card\"><header><h1>";
  html += title;
  html += "</h1></header>";
  if (showNav) {
    html += getNavHtml(activeHome, activeHistory, activeTemperature, activeProfile, activeConsole);
  }
  html += bodyContent;
  html += "</div></body></html>";
  return html;
}

void fillNavTokens(web_storage::TemplateToken* tokens,
                   const char* activeHome,
                   const char* activeHistory,
                   const char* activeTemperature,
                   const char* activeProfile,
                   const char* activeConsole) {
  tokens[0] = {"{{ACTIVE_HOME}}", String(activeHome)};
  tokens[1] = {"{{ACTIVE_HISTORY}}", String(activeHistory)};
  tokens[2] = {"{{ACTIVE_TEMPERATURE}}", String(activeTemperature)};
  tokens[3] = {"{{ACTIVE_PROFILE}}", String(activeProfile)};
  tokens[4] = {"{{ACTIVE_CONSOLE}}", String(activeConsole)};
}

String buildNoticeHtml(const String& statusMessage) {
  if (statusMessage.length() == 0) {
    return String();
  }

  const bool isError = statusMessage.startsWith("ERR:");
  const String message = isError ? statusMessage.substring(4) : statusMessage;
  const String cssClass = isError ? "notice error" : "notice success";

  return String("<div class=\"") + cssClass + "\">" + message + "</div>";
}

String buildFallbackHomeHtml(const AppState& state) {
  const String content = String("<main><h2>Accueil</h2><p>Mode Web actif sur l'ESP32-C3.</p><div class=\"section\"><p>SSID : ") +
                         web_config::WIFI_SSID +
                         "</p><p>URL locale : " +
                         web_config::WEB_HOST_URL +
                         "</p><p>Port : " +
                         String(web_config::WEB_PORT) +
                         "</p><p>Niveau LOG : " +
                         getCurrentLogLevelName(state.webLogLevel) +
                         "</p><p>Refresh LOG : " +
                         String(state.webLogRefreshSeconds) +
                         "s" +
                         "</p><p>Profil utilise : " +
                         getProfileName(state.activeProfile) +
                         "</p></div></main>";
  return wrapFallbackPage(web_config::WEB_TITLE, content, true, "active", "", "", "", "");
}

String buildFallbackHistoryHtml() {
  const String content = R"HISTORY(<main><h2>Historique PWM</h2><div class="grid cols-2"><section class="section"><h3>Valeurs en direct</h3><p>PWM actuel : <strong id="pwmCurrent">-- %</strong></p><p>PWM max : <strong id="pwmMax">-- %</strong></p><p>Frequence : <strong id="pwmFrequency">-- Hz</strong></p><p class="inline-help" id="lastUpdate">Derniere mise a jour: --</p></section><section class="section"><h3>Graphique des derniers echantillons</h3><div class="grid cols-2"><p><label for="rangeSelect">Plage affichee</label><select id="rangeSelect"><option value="60">1 min</option><option value="300">5 min</option><option value="600">10 min</option><option value="all">Tous</option></select></p><p><label for="yMaxInput">Axe Y max (%)</label><input id="yMaxInput" type="number" min="10" max="100" step="5" value="70"></p></div><canvas id="pwmChart" width="900" height="320" style="width:100%;height:auto;display:block;"></canvas><p class="inline-help">Le graphe conserve un point par seconde pendant toute la session Web. La plage 1 min decale automatiquement la fenetre au fil du temps.</p></section></div></main><script>const currentEl=document.getElementById('pwmCurrent');const maxEl=document.getElementById('pwmMax');const freqEl=document.getElementById('pwmFrequency');const updateEl=document.getElementById('lastUpdate');const rangeSelect=document.getElementById('rangeSelect');const yMaxInput=document.getElementById('yMaxInput');const canvas=document.getElementById('pwmChart');const ctx=canvas.getContext('2d');const sessionStartMs=Date.now();const samples=[];let refreshInFlight=false;const RANGE_TICKS={'60':5,'300':30,'600':60};function getElapsedSeconds(){return Math.max(0,Math.floor((Date.now()-sessionStartMs)/1000));}function upsertSample(second,value){const numericValue=Number.isFinite(value)?value:Number.NaN;const lastSample=samples[samples.length-1];if(lastSample&&lastSample.second===second){lastSample.value=numericValue;return;}samples.push({second,value:numericValue});}function parseRangeSeconds(){return rangeSelect.value==='all'?null:Number(rangeSelect.value);}function sanitizeYAxisMax(){const parsed=Number(yMaxInput.value);if(!Number.isFinite(parsed)){return 70;}const clamped=Math.min(100,Math.max(10,parsed));return Math.round(clamped/5)*5;}function formatElapsedLabel(totalSeconds){const rounded=Math.max(0,Math.floor(totalSeconds));const minutes=Math.floor(rounded/60);const seconds=rounded%60;return minutes>0?`${minutes}m${String(seconds).padStart(2,'0')}`:`${seconds}s`;}function getTickIntervalSeconds(rangeSeconds,spanSeconds){if(rangeSeconds!==null){return RANGE_TICKS[String(rangeSeconds)]||60;}const candidates=[5,10,15,30,60,120,300,600];const targetTicks=8;for(const candidate of candidates){if(spanSeconds/candidate<=targetTicks){return candidate;}}return 600;}function formatPercent(value){return Number.isFinite(value)?`${value.toFixed(1)} %`:'-- %';}function formatFrequency(value){return Number.isFinite(value)?`${value.toFixed(0)} Hz`:'-- Hz';}function drawChart(){const width=canvas.width;const height=canvas.height;ctx.clearRect(0,0,width,height);ctx.fillStyle='rgba(3, 8, 15, 0.85)';ctx.fillRect(0,0,width,height);const left=58;const right=width-14;const top=16;const bottom=height-36;const graphWidth=right-left;const graphHeight=bottom-top;const elapsedSeconds=getElapsedSeconds();const rangeSeconds=parseRangeSeconds();const visibleStart=rangeSeconds===null?0:Math.max(0,elapsedSeconds-rangeSeconds);const visibleEnd=Math.max(1,elapsedSeconds);const spanSeconds=Math.max(1,visibleEnd-visibleStart);const yMax=sanitizeYAxisMax();const visibleSamples=samples.filter((sample)=>sample.second>=visibleStart&&sample.second<=visibleEnd);const values=visibleSamples.map((sample)=>sample.value).filter((value)=>Number.isFinite(value));if(values.length===0){ctx.fillStyle='#a2b4cd';ctx.font='16px sans-serif';ctx.fillText('Aucune donnee historique disponible',left,top+24);return;}ctx.strokeStyle='rgba(135, 164, 205, 0.24)';ctx.lineWidth=1;ctx.fillStyle='#a2b4cd';ctx.font='12px monospace';for(let tickValue=0;tickValue<=yMax;tickValue+=5){const ratio=tickValue/yMax;const y=bottom-ratio*graphHeight;ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();ctx.fillText(`${tickValue}%`,6,y+4);}ctx.strokeStyle='rgba(135, 164, 205, 0.48)';ctx.beginPath();ctx.moveTo(left,top);ctx.lineTo(left,bottom);ctx.lineTo(right,bottom);ctx.stroke();const tickInterval=getTickIntervalSeconds(rangeSeconds,spanSeconds);const firstTick=Math.ceil(visibleStart/tickInterval)*tickInterval;for(let tickSecond=firstTick;tickSecond<=visibleEnd;tickSecond+=tickInterval){const x=left+((tickSecond-visibleStart)/spanSeconds)*graphWidth;ctx.strokeStyle='rgba(135, 164, 205, 0.36)';ctx.beginPath();ctx.moveTo(x,top);ctx.lineTo(x,bottom);ctx.stroke();ctx.fillStyle='#a2b4cd';ctx.fillText(formatElapsedLabel(tickSecond),Math.max(left,x-16),height-10);}ctx.strokeStyle='#4ad2ff';ctx.lineWidth=2;ctx.beginPath();let started=false;for(const sample of visibleSamples){const value=sample.value;if(!Number.isFinite(value)){started=false;continue;}const clampedValue=Math.max(0,Math.min(yMax,value));const x=left+((sample.second-visibleStart)/spanSeconds)*graphWidth;const y=bottom-(clampedValue/yMax)*graphHeight;if(!started){ctx.moveTo(x,y);started=true;}else{ctx.lineTo(x,y);}}ctx.stroke();ctx.fillStyle='#a2b4cd';ctx.font='13px monospace';ctx.fillText(`Fenetre: ${formatElapsedLabel(visibleStart)} -> ${formatElapsedLabel(visibleEnd)}`,left,height-18);}async function refreshHistory(){if(refreshInFlight){return;}refreshInFlight=true;try{const response=await fetch('/api/history',{cache:'no-store'});if(!response.ok){throw new Error(`HTTP ${response.status}`);}const payload=await response.json();const current=Number(payload.current);const max=Number(payload.max);const frequency=Number(payload.frequency_hz);currentEl.textContent=formatPercent(current);maxEl.textContent=formatPercent(max);freqEl.textContent=formatFrequency(frequency);updateEl.textContent=`Derniere mise a jour: ${new Date().toLocaleTimeString()}`;upsertSample(getElapsedSeconds(),current);drawChart();}catch(error){updateEl.textContent=`Erreur de lecture: ${error.message}`;}finally{refreshInFlight=false;}}rangeSelect.addEventListener('change',drawChart);yMaxInput.addEventListener('change',()=>{yMaxInput.value=String(sanitizeYAxisMax());drawChart();});refreshHistory();setInterval(refreshHistory,1000);</script>)HISTORY";
  return wrapFallbackPage("Historique PWM", content, true, "", "active", "", "", "");
}

String buildFallbackTemperatureHtml() {
  const String content = R"TEMPERATURE(<main><h2>Temperature</h2><p>Ci-dessous le graphique des derniers echantillons de temperature des sondes si elle sont actives.</p><p><label for="rangeSelect">Plage affichee</label><select id="rangeSelect"><option value="60">1 min</option><option value="300">5 min</option><option value="600">10 min</option><option value="all">Tous</option></select></p><h3>Sonde 1</h3><div class="grid cols-2"><section class="section"><h3>Valeurs en direct</h3><p>Temperature actuel : <strong id="temperatureCurrent1">-- %</strong></p><p>Temperature max : <strong id="temperatureMax1">-- %</strong></p><p class="inline-help" id="lastUpdate">Derniere mise a jour: --</p></section><section class="section"><h4>Graphique des derniers echantillons</h4><div class="grid cols-2"><p><label for="yMaxInputTemperature">Axe Y max (°C)</label><input id="yMaxInputTemperature" type="number" min="40" max="120" step="5" value="100"></p></div><canvas id="temperatureChart1" width="900" height="320" style="width:100%;height:auto;display:block;"></canvas><p class="inline-help">Le graphe conserve un point par seconde pendant toute la session Web. La plage 1 min decale automatiquement la fenetre au fil du temps.</p></section></div><p></p><h3>Sonde 2</h3><div class="grid cols-2"><section class="section"><h3>Valeurs en direct</h3><p>Temperature actuel : <strong id="temperatureCurrent2">-- %</strong></p><p>Temperature max : <strong id="temperatureMax2">-- %</strong></p><p class="inline-help" id="lastUpdate">Derniere mise a jour: --</p></section><section class="section"><h4>Graphique des derniers echantillons</h4><div class="grid cols-2"><p><label for="yMaxInputTemperature">Axe Y max (°C)</label><input id="yMaxInputTemperature" type="number" min="40" max="120" step="5" value="100"></p></div><canvas id="temperatureChart2" width="900" height="320" style="width:100%;height:auto;display:block;"></canvas><p class="inline-help">Le graphe conserve un point par seconde pendant toute la session Web. La plage 1 min decale automatiquement la fenetre au fil du temps.</p></section></div></main>)TEMPERATURE";
  return wrapFallbackPage("Temperature", content, true, "", "", "active", "", "");
}

String buildLoginFallbackHtml(const String& noticeHtml) {
  String html;
  html.reserve(2200 + noticeHtml.length());
  html += "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>";
  html += web_config::WEB_TITLE;
  html += "</title><link rel=\"stylesheet\" href=\"/assets/style.css\"></head><body><div class=\"login-wrap\"><div class=\"card login-card\"><h1>";
  html += web_config::WEB_TITLE;
  html += "</h1><h2>Authentification requise</h2>";
  html += noticeHtml;
  html += "<form method=\"post\" action=\"/login\"><label for=\"password\">Mot de passe</label><div class=\"field\"><input id=\"password\" name=\"password\" type=\"password\" autocomplete=\"off\" required><button type=\"button\" id=\"togglePassword\">Afficher</button></div><p class=\"inline-help\">Le mot de passe Web est defini dans la configuration ESP32.</p><div class=\"btn-row\"><button class=\"primary\" type=\"submit\">Valider</button></div></form></div></div><script>const passwordInput=document.getElementById('password');const togglePassword=document.getElementById('togglePassword');if(passwordInput&&togglePassword){togglePassword.addEventListener('click',()=>{const isHidden=passwordInput.type==='password';passwordInput.type=isHidden?'text':'password';togglePassword.textContent=isHidden?'Masquer':'Afficher';});}</script></body></html>";
  return html;
}

String buildFallbackProfileHtml(const AppState& state, const String& statusMessage) {
  auto buildProfileOption = [&](ConsoleProfile profile, const char* label) {
    const bool selected = profile == state.activeProfile;
    return String("<option value=\"") + static_cast<uint8_t>(profile) +
           "\"" + (selected ? " selected" : "") + ">" + label + "</option>";
  };

  const String profileOptions =
      buildProfileOption(ConsoleProfile::PS3_FAT, "PS3 FAT") +
      buildProfileOption(ConsoleProfile::PS4_PRO, "PS4 PRO") +
      buildProfileOption(ConsoleProfile::PS5_FAT, "PS5 FAT");

  const uint8_t normalizedMode = state.iconMode == 0 ? 0 : 1;
  const String modeOptions =
      String("<option value=\"0\"") + (normalizedMode == 0 ? " selected" : "") + ">simple</option>" +
      String("<option value=\"1\"") + (normalizedMode == 1 ? " selected" : "") + ">smiley</option>";

  const uint8_t profileIndex = static_cast<uint8_t>(state.activeProfile);
  const String thresholdsJson = buildProfileThresholdsJson(state);
  char idleCoolBuffer[16];
  char gameCoolBuffer[16];
    char gameHotBuffer[16];
  snprintf(idleCoolBuffer, sizeof(idleCoolBuffer), "%.1f", state.profileThresholds[profileIndex].idleCoolMax);
  snprintf(gameCoolBuffer, sizeof(gameCoolBuffer), "%.1f", state.profileThresholds[profileIndex].gameCoolMax);
    snprintf(gameHotBuffer, sizeof(gameHotBuffer), "%.1f", state.profileThresholds[profileIndex].gameHotMax);

  String content;
  content.reserve(2800);
  content += "<main><h2>Page Profil</h2><p>Modifie les parametres utilises par l'ESP32-C3, puis applique immediatement.</p>";
  content += buildNoticeHtml(statusMessage);
  content += "<div class=\"grid cols-2\"><section class=\"section\"><h3>1. Profil utilise</h3><form method=\"post\" action=\"/profile/apply\" onsubmit=\"return confirm('Sauvegarder puis redemarrer l\\'ESP32-C3 pour appliquer les changements ?');\"><label for=\"profile\">Profil console</label><select id=\"profile\" name=\"profile\">";
  content += profileOptions;
  content += "</select><label for=\"idle_cool_max\" title=\"";
  content += WEB_IDLE_COOL_MAX_DESCRIPTION;
  content += "\">IDLE_COOL_MAX (%)</label><input id=\"idle_cool_max\" name=\"idle_cool_max\" type=\"number\" step=\"0.5\" min=\"0\" max=\"100\" value=\"";
  content += String(idleCoolBuffer);
  content += "\" required><label for=\"game_cool_max\" title=\"";
  content += WEB_GAME_COOL_MAX_DESCRIPTION;
  content += "\">GAME_COOL_MAX (%)</label><input id=\"game_cool_max\" name=\"game_cool_max\" type=\"number\" step=\"0.5\" min=\"0\" max=\"100\" value=\"";
  content += String(gameCoolBuffer);
  content += "\" required><label for=\"game_hot_max\" title=\"";
  content += WEB_GAME_HOT_MAX_DESCRIPTION;
  content += "\">GAME_HOT_MAX (%)</label><input id=\"game_hot_max\" name=\"game_hot_max\" type=\"number\" step=\"0.5\" min=\"0\" max=\"100\" value=\"";
  content += String(gameHotBuffer);
  content += "\" required><p class=\"inline-help\">Survoler les labels pour afficher la description de chaque seuil.</p><label for=\"icon_mode\">2. Images OLED</label><select id=\"icon_mode\" name=\"icon_mode\">";
  content += modeOptions;
  content += "</select><p class=\"inline-help\">simple : texte seul ; smiley : visages selon l'etat.</p><div class=\"btn-row\"><button class=\"primary\" type=\"submit\">Appliquer les parametres</button></div></form></section><section class=\"section\"><h3>3. Remise a zero</h3><p>Reinitialise la configuration sauvegardee, puis redemarre l'ESP32-C3.</p><form method=\"post\" action=\"/profile/reset\" onsubmit=\"return confirm('Confirmer la remise a zero ? Cette action est irreversible.');\"><div class=\"btn-row\"><button class=\"danger\" type=\"submit\">Remise a zero + Redemarrage</button></div></form></section></div></main>";
  content += "<script>const profileSelect=document.getElementById('profile');const idleInput=document.getElementById('idle_cool_max');const coolInput=document.getElementById('game_cool_max');const hotInput=document.getElementById('game_hot_max');const thresholdsByProfile=";
  content += thresholdsJson;
  content += ";function applyThresholdsForProfile(profileValue){const thresholds=thresholdsByProfile[String(profileValue)];if(!thresholds){return;}idleInput.value=Number(thresholds.idle).toFixed(1);coolInput.value=Number(thresholds.cool).toFixed(1);hotInput.value=Number(thresholds.hot).toFixed(1);}if(profileSelect&&idleInput&&coolInput&&hotInput){profileSelect.addEventListener('change',()=>applyThresholdsForProfile(profileSelect.value));}</script>";
  return wrapFallbackPage("Profil", content, true, "", "", "", "active", "");
}

String buildFallbackConsoleHtml(const String& title,
                                const String& logContent,
                                uint16_t refreshSeconds,
                                uint8_t currentLogLevel,
                                const String& statusMessage) {
  const String levelOptions =
      logLevelToOption(static_cast<uint8_t>(log_config::LogLevel::TRACE), currentLogLevel, "TRACE") +
      logLevelToOption(static_cast<uint8_t>(log_config::LogLevel::DEBUG), currentLogLevel, "DEBUG") +
      logLevelToOption(static_cast<uint8_t>(log_config::LogLevel::INFO), currentLogLevel, "INFO") +
      logLevelToOption(static_cast<uint8_t>(log_config::LogLevel::WARN), currentLogLevel, "WARN") +
      logLevelToOption(static_cast<uint8_t>(log_config::LogLevel::ERROR), currentLogLevel, "ERROR");
  const String refreshOptions =
      refreshToOption(1, refreshSeconds) +
      refreshToOption(2, refreshSeconds) +
      refreshToOption(5, refreshSeconds);

  String content;
  content.reserve(2048 + logContent.length() + statusMessage.length());
  content += "<main><meta http-equiv=\"refresh\" content=\"";
  content += String(refreshSeconds);
  content += "; url=/console\">";
  content += buildNoticeHtml(statusMessage);
  content += "<section class=\"section\"><form method=\"post\" action=\"/console/settings\" onsubmit=\"return confirm('Enregistrer les parametres de logs puis redemarrer l\\'ESP32-C3 ?');\"><div class=\"grid cols-2\"><div><label for=\"log_level\">Niveau LOG</label><select id=\"log_level\" name=\"log_level\">";
  content += levelOptions;
  content += "</select></div><div><label for=\"refresh_seconds\">Refresh Console</label><select id=\"refresh_seconds\" name=\"refresh_seconds\">";
  content += refreshOptions;
  content += "</select></div></div><div class=\"btn-row\"><button class=\"primary\" type=\"submit\">Enregistrer + Redemarrer</button></div></form></section><div class=\"log-box\">";
  content += logContent.length() > 0 ? logContent : String("<p>Aucun log pour le moment.</p>");
  content += "</div></main>";
  return wrapFallbackPage(title, content, true, "", "", "", "", "active");
}

String profileToOption(const ConsoleProfile profile, const ConsoleProfile activeProfile, const char* label) {
  const bool selected = profile == activeProfile;
  return String("<option value=\"") + static_cast<uint8_t>(profile) + "\"" + (selected ? " selected" : "") + ">" + label + "</option>";
}

String iconModeToOption(const uint8_t mode, const uint8_t activeMode, const char* label) {
  const bool selected = mode == activeMode;
  return String("<option value=\"") + mode + "\"" + (selected ? " selected" : "") + ">" + label + "</option>";
}

String logLevelToOption(const uint8_t value, const uint8_t selectedValue, const char* label) {
  const bool selected = value == selectedValue;
  return String("<option value=\"") + value + "\"" + (selected ? " selected" : "") + ">" + label + "</option>";
}

String refreshToOption(const uint16_t value, const uint16_t selectedValue) {
  const bool selected = value == selectedValue;
  return String("<option value=\"") + value + "\"" + (selected ? " selected" : "") + ">" + value + "s</option>";
}

String formatThresholdValue(float thresholdOffset) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%.1f", thresholdOffset);
  return String(buffer);
}

}  // namespace

bool web_pages::initializeStorage() {
  return web_storage::initialize();
}

bool web_pages::isStorageReady() {
  return web_storage::isReady();
}

String web_pages::getSharedCss() {
  return String(R"CSS(
:root {
  --bg-a: #0b1220;
  --bg-b: #1c2637;
  --panel: rgba(12, 18, 30, 0.9);
  --line: rgba(135, 164, 205, 0.28);
  --text: #e7eef8;
  --muted: #a2b4cd;
  --accent: #4ad2ff;
  --accent-strong: #2ea4dd;
  --danger: #ff8c84;
  --ok: #8dffb2;
}
* { box-sizing: border-box; }
html, body { margin: 0; }
body {
  min-height: 100vh;
  padding: 16px;
  color: var(--text);
  font-family: "Segoe UI", "Trebuchet MS", Tahoma, sans-serif;
  background:
    radial-gradient(circle at 10% 10%, rgba(74, 210, 255, 0.18), transparent 45%),
    radial-gradient(circle at 90% 80%, rgba(141, 255, 178, 0.08), transparent 45%),
    linear-gradient(140deg, var(--bg-a), var(--bg-b));
}
.card {
  max-width: 960px;
  margin: 0 auto;
  background: var(--panel);
  border: 1px solid var(--line);
  border-radius: 16px;
  box-shadow: 0 16px 36px rgba(0, 0, 0, 0.32);
  padding: 18px;
}
h1, h2, h3 { margin: 0 0 12px; }
h1 { font-size: 1.5rem; }
h2 { font-size: 1.15rem; }
h3 { font-size: 1rem; color: var(--muted); }
p { margin: 0 0 10px; color: var(--muted); }
nav { display: flex; gap: 10px; flex-wrap: wrap; margin-bottom: 16px; }
nav a {
  color: #052337;
  background: var(--accent);
  border: 1px solid transparent;
  border-radius: 10px;
  text-decoration: none;
  font-weight: 700;
  padding: 8px 12px;
}
nav a.active { background: transparent; color: var(--text); border-color: var(--line); }
.grid { display: grid; gap: 12px; }
@media (min-width: 760px) { .grid.cols-2 { grid-template-columns: 1fr 1fr; } }
.section {
  border: 1px solid var(--line);
  border-radius: 12px;
  background: rgba(8, 14, 24, 0.8);
  padding: 12px;
}
label { display: block; font-weight: 700; font-size: 0.92rem; margin-bottom: 6px; }
input, select, button {
  width: 100%;
  border: 1px solid var(--line);
  border-radius: 10px;
  padding: 10px;
  font-size: 0.95rem;
  background: rgba(3, 8, 15, 0.8);
  color: var(--text);
}
input:focus, select:focus { outline: 2px solid rgba(74, 210, 255, 0.4); outline-offset: 1px; }
.field {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 12px;
}
.field input { flex: 1; }
.field button {
  width: auto;
  white-space: nowrap;
  background: rgba(74, 210, 255, 0.14);
}
.btn-row { display: flex; gap: 10px; flex-wrap: wrap; }
.btn-row button { width: auto; min-width: 190px; }
button.primary {
  background: linear-gradient(140deg, var(--accent), var(--accent-strong));
  color: #052337;
  font-weight: 800;
  border: 0;
}
button.danger {
  background: linear-gradient(140deg, #ff8c84, #ff6b60);
  color: #2f1010;
  font-weight: 800;
  border: 0;
}
.notice { padding: 10px 12px; border-radius: 10px; margin-bottom: 12px; border: 1px solid var(--line); }
.notice.error { color: #ffd4d1; border-color: rgba(255, 140, 132, 0.45); background: rgba(123, 37, 37, 0.3); }
.notice.success { color: #d5ffe2; border-color: rgba(141, 255, 178, 0.4); background: rgba(21, 77, 44, 0.3); }
.login-wrap { min-height: calc(100vh - 32px); display: grid; place-items: center; }
.login-card { width: min(100%, 430px); }
.inline-help { color: var(--muted); font-size: 0.86rem; }
.log-box {
  background: rgba(3, 8, 15, 0.85);
  border: 1px solid var(--line);
  border-radius: 12px;
  padding: 12px;
  max-height: 62vh;
  overflow: auto;
  font-family: "Consolas", "Courier New", monospace;
  font-size: 0.86rem;
}
.log-box div { margin-bottom: 4px; white-space: pre-wrap; }
)CSS");
}

String web_pages::getLoginPage(const String& errorMessage) {
  const String notice = errorMessage.length() > 0
                            ? String("<div class=\"notice error\">") + errorMessage + "</div>"
                            : String();

  const web_storage::TemplateToken tokens[] = {
      {"{{TITLE}}", String(web_config::WEB_TITLE)},
      {"{{NOTICE}}", notice},
  };

  const String rendered = web_storage::renderTemplate(kPageLogin, tokens, sizeof(tokens) / sizeof(tokens[0]));
  if (rendered.length() > 0) {
    return rendered;
  }

  return buildLoginFallbackHtml(notice);
}

String web_pages::getHomePage(const AppState& state) {
  web_storage::TemplateToken navTokens[4];
  fillNavTokens(navTokens, "active", "", "", "", "");

  const web_storage::TemplateToken tokens[] = {
      {"{{TITLE}}", String(web_config::WEB_TITLE)},
      navTokens[0],
      navTokens[1],
      navTokens[2],
      navTokens[3],
      navTokens[4],
      {"{{SSID}}", String(web_config::WIFI_SSID)},
      {"{{WEB_URL}}", String(web_config::WEB_HOST_URL)},
      {"{{WEB_PORT}}", String(web_config::WEB_PORT)},
      {"{{LOG_LEVEL}}", String(getCurrentLogLevelName(state.webLogLevel))},
      {"{{LOG_REFRESH_SECONDS}}", String(state.webLogRefreshSeconds)},
      {"{{ACTIVE_PROFILE}}", String(getProfileName(state.activeProfile))},
  };

  const String rendered = web_storage::renderTemplate(kPageHome, tokens, sizeof(tokens) / sizeof(tokens[0]));
  if (rendered.length() > 0) {
    return rendered;
  }

  return buildFallbackHomeHtml(state);
}

String web_pages::getHistoryPage() {
  web_storage::TemplateToken navTokens[4];
  fillNavTokens(navTokens, "", "active", "", "", "");

  const web_storage::TemplateToken tokens[] = {
      {"{{TITLE}}", String("Historique PWM")},
      navTokens[0],
      navTokens[1],
      navTokens[2],
      navTokens[3],
      navTokens[4],
  };

  const String rendered = web_storage::renderTemplate(kPageHistory, tokens, sizeof(tokens) / sizeof(tokens[0]));
  if (rendered.length() > 0) {
    return rendered;
  }

  return buildFallbackHistoryHtml();
}

String web_pages::getTemperaturePage() {
  web_storage::TemplateToken navTokens[4];
  fillNavTokens(navTokens, "", "", "active", "", "");

  const web_storage::TemplateToken tokens[] = {
      {"{{TITLE}}", String("Temperature")},
      navTokens[0],
      navTokens[1],
      navTokens[2],
      navTokens[3],
      navTokens[4],
  };

  const String rendered = web_storage::renderTemplate(kPageTemperature, tokens, sizeof(tokens) / sizeof(tokens[0]));
  if (rendered.length() > 0) {
    return rendered;
  }

  return buildFallbackTemperatureHtml();
}

String web_pages::getProfilePage(const AppState& state, const String& statusMessage) {
  const String profileOptions =
      profileToOption(ConsoleProfile::PS3_FAT, state.activeProfile, "PS3 FAT") +
      profileToOption(ConsoleProfile::PS4_PRO, state.activeProfile, "PS4 PRO") +
      profileToOption(ConsoleProfile::PS5_FAT, state.activeProfile, "PS5 FAT");

  const uint8_t normalizedMode = state.iconMode == 0 ? 0 : 1;
  const uint8_t profileIndex = static_cast<uint8_t>(state.activeProfile);
  const String thresholdsJson = buildProfileThresholdsJson(state);
  const String modeOptions =
      iconModeToOption(0, normalizedMode, "simple") +
      iconModeToOption(1, normalizedMode, "smiley");

    web_storage::TemplateToken navTokens[4];
    fillNavTokens(navTokens, "", "", "", "active", "");

    const web_storage::TemplateToken tokens[] = {
      {"{{TITLE}}", String("Profil")},
      navTokens[0],
      navTokens[1],
      navTokens[2],
      navTokens[3],
      navTokens[4],
      {"{{NOTICE}}", buildNoticeHtml(statusMessage)},
      {"{{PROFILE_OPTIONS}}", profileOptions},
      {"{{IDLE_COOL_MAX}}", formatThresholdValue(state.profileThresholds[profileIndex].idleCoolMax)},
      {"{{GAME_COOL_MAX}}", formatThresholdValue(state.profileThresholds[profileIndex].gameCoolMax)},
      {"{{GAME_HOT_MAX}}", formatThresholdValue(state.profileThresholds[profileIndex].gameHotMax)},
      {"{{TIP_IDLE_COOL_MAX}}", String(WEB_IDLE_COOL_MAX_DESCRIPTION)},
      {"{{TIP_GAME_COOL_MAX}}", String(WEB_GAME_COOL_MAX_DESCRIPTION)},
      {"{{TIP_GAME_HOT_MAX}}", String(WEB_GAME_HOT_MAX_DESCRIPTION)},
      {"{{ICON_MODE_OPTIONS}}", modeOptions},
      {"{{PROFILE_THRESHOLDS_JSON}}", thresholdsJson},
    };

  const String rendered = web_storage::renderTemplate(kPageProfile, tokens, sizeof(tokens) / sizeof(tokens[0]));
  if (rendered.length() > 0) {
    return rendered;
  }

  return buildFallbackProfileHtml(state, statusMessage);
}

String web_pages::getConsolePage(const String& title,
                 const String& logContent,
                 const uint16_t refreshSeconds,
                 const uint8_t currentLogLevel,
                 const String& statusMessage) {
  web_storage::TemplateToken navTokens[4];
  fillNavTokens(navTokens, "", "", "", "", "active");

  const String levelOptions =
    logLevelToOption(static_cast<uint8_t>(log_config::LogLevel::TRACE), currentLogLevel, "TRACE") +
    logLevelToOption(static_cast<uint8_t>(log_config::LogLevel::DEBUG), currentLogLevel, "DEBUG") +
    logLevelToOption(static_cast<uint8_t>(log_config::LogLevel::INFO), currentLogLevel, "INFO") +
    logLevelToOption(static_cast<uint8_t>(log_config::LogLevel::WARN), currentLogLevel, "WARN") +
    logLevelToOption(static_cast<uint8_t>(log_config::LogLevel::ERROR), currentLogLevel, "ERROR");

  const String refreshOptions =
    refreshToOption(1, refreshSeconds) +
    refreshToOption(2, refreshSeconds) +
    refreshToOption(5, refreshSeconds);

  const web_storage::TemplateToken tokens[] = {
      {"{{TITLE}}", title},
      navTokens[0],
      navTokens[1],
      navTokens[2],
      navTokens[3],
    {"{{NOTICE}}", buildNoticeHtml(statusMessage)},
    {"{{LOG_LEVEL_OPTIONS}}", levelOptions},
    {"{{REFRESH_OPTIONS}}", refreshOptions},
      {"{{REFRESH_SECONDS}}", String(refreshSeconds)},
      {"{{LOG_CONTENT}}", logContent.length() > 0 ? logContent : String("<p>Aucun log pour le moment.</p>")},
  };

  const String rendered = web_storage::renderTemplate(kPageConsole, tokens, sizeof(tokens) / sizeof(tokens[0]));
  if (rendered.length() > 0) {
    return rendered;
  }

  return buildFallbackConsoleHtml(title, logContent, refreshSeconds, currentLogLevel, statusMessage);
}

String web_pages::getRedirectPage(const char* url) {
  const web_storage::TemplateToken tokens[] = {
      {"{{REDIRECT_URL}}", String(url)},
  };

  const String rendered = web_storage::renderTemplate(
      kPageRedirect,
      tokens,
      sizeof(tokens) / sizeof(tokens[0]));
  if (rendered.length() > 0) {
    return rendered;
  }

  return String("<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta http-equiv=\"refresh\" content=\"0; url=") +
         url +
         "\"><title>Redirection</title></head><body><p>Redirection...</p></body></html>";
}
