#include "session_stats.h"

#include "app/app_state.h"
#include "display_ui.h"
#include "utils/logger.h"

namespace session_stats {
namespace {

int16_t trendBuffer[3][LIVE_POINTS];
uint16_t trendHead = 0;  // Prochain emplacement d'ecriture.
uint16_t trendLength = 0;

double bucketDutySum = 0.0;
uint32_t bucketDutyCount = 0;
uint32_t bucketStartMs = 0;

uint32_t lastUpdateMs = 0;

bool sessionActive = false;
uint32_t candidateStartMs = 0;
uint32_t sessionStartMs = 0;
uint32_t lastActiveMs = 0;
double dutyTimeSum = 0.0;
uint32_t sessionTotalMs = 0;
uint32_t hotMs = 0;
float sessionMaxDuty = 0.0f;
float sessionMaxTemp[2] = {NAN, NAN};

Summary lastSummary;

int16_t toTenths(float value) {
  return isnan(value) ? kInvalid : static_cast<int16_t>(lroundf(value * 10.0f));
}

void pushTrendPoint(int16_t duty, int16_t t0, int16_t t1) {
  trendBuffer[0][trendHead] = duty;
  trendBuffer[1][trendHead] = t0;
  trendBuffer[2][trendHead] = t1;
  trendHead = (trendHead + 1) % LIVE_POINTS;
  if (trendLength < LIVE_POINTS) {
    ++trendLength;
  }
}

Summary buildSummary(bool active, uint32_t endMs) {
  Summary summary;
  summary.valid = true;
  summary.active = active;
  summary.durationS = (endMs - sessionStartMs) / 1000UL;
  summary.avgDuty = sessionTotalMs > 0 ? static_cast<float>(dutyTimeSum / sessionTotalMs) : 0.0f;
  summary.maxDuty = sessionMaxDuty;
  summary.maxTemp[0] = sessionMaxTemp[0];
  summary.maxTemp[1] = sessionMaxTemp[1];
  summary.hotSeconds = hotMs / 1000UL;
  return summary;
}

void startSession(uint32_t startMs) {
  sessionActive = true;
  sessionStartMs = startMs;
  dutyTimeSum = 0.0;
  sessionTotalMs = 0;
  hotMs = 0;
  sessionMaxDuty = 0.0f;
  sessionMaxTemp[0] = NAN;
  sessionMaxTemp[1] = NAN;
  logger::info("[STATS] game session started");
}

void updateTrend(uint32_t nowMs, const AppState& state) {
  if (state.pwmDetected) {
    bucketDutySum += state.dutyFiltered;
    ++bucketDutyCount;
  }

  if (bucketStartMs == 0) {
    bucketStartMs = nowMs;
    return;
  }
  if (nowMs - bucketStartMs < LIVE_SAMPLE_S * 1000UL) {
    return;
  }

  const int16_t duty = bucketDutyCount > 0
                           ? toTenths(static_cast<float>(bucketDutySum / bucketDutyCount))
                           : kInvalid;
  const int16_t t0 = state.sensorConfig[0].enabled || state.sensorConfig[0].detected
                         ? toTenths(state.sensorConfig[0].currentTemperature)
                         : kInvalid;
  const int16_t t1 = state.sensorConfig[1].enabled || state.sensorConfig[1].detected
                         ? toTenths(state.sensorConfig[1].currentTemperature)
                         : kInvalid;
  pushTrendPoint(duty, t0, t1);

  bucketStartMs = nowMs;
  bucketDutySum = 0.0;
  bucketDutyCount = 0;
}

void updateSession(uint32_t nowMs, uint32_t dtMs, const AppState& state) {
  const bool above = state.pwmDetected && state.dutyFiltered > getActiveIdleCoolMax();

  if (!sessionActive) {
    if (!above) {
      candidateStartMs = 0;
      return;
    }
    if (candidateStartMs == 0) {
      candidateStartMs = nowMs;
    }
    if (nowMs - candidateStartMs >= SESSION_START_MS) {
      startSession(candidateStartMs);
      lastActiveMs = nowMs;
      candidateStartMs = 0;
    }
    return;
  }

  if (above) {
    lastActiveMs = nowMs;
  }

  if (state.pwmDetected) {
    dutyTimeSum += static_cast<double>(state.dutyFiltered) * dtMs;
    sessionTotalMs += dtMs;
    sessionMaxDuty = max(sessionMaxDuty, state.dutyFiltered);
    if (state.dutyFiltered > getActiveGameHotMax()) {
      hotMs += dtMs;
    }
  }

  for (uint8_t i = 0; i < 2; ++i) {
    const float temperature = state.sensorConfig[i].currentTemperature;
    if (!isnan(temperature) && (isnan(sessionMaxTemp[i]) || temperature > sessionMaxTemp[i])) {
      sessionMaxTemp[i] = temperature;
    }
  }

  if (nowMs - lastActiveMs >= SESSION_END_MS) {
    lastSummary = buildSummary(false, lastActiveMs);
    sessionActive = false;
    logger::infof("[STATS] game session ended: %lus avg=%.1f%% max=%.1f%%",
                  static_cast<unsigned long>(lastSummary.durationS),
                  lastSummary.avgDuty,
                  lastSummary.maxDuty);
  }
}

}  // namespace

void update(uint32_t nowMs) {
  const auto& state = getAppState();
  const uint32_t dtMs = lastUpdateMs == 0 ? 0 : min<uint32_t>(nowMs - lastUpdateMs, 1000UL);
  lastUpdateMs = nowMs;

  updateTrend(nowMs, state);
  updateSession(nowMs, dtMs, state);
}

Summary currentSession() {
  if (!sessionActive) {
    return Summary();
  }
  return buildSummary(true, millis());
}

Summary lastSession() {
  return lastSummary;
}

uint16_t trendCount() {
  return trendLength;
}

static int16_t trendValue(uint8_t series, uint16_t index) {
  if (index >= trendLength) {
    return kInvalid;
  }
  const uint16_t oldest = (trendHead + LIVE_POINTS - trendLength) % LIVE_POINTS;
  return trendBuffer[series][(oldest + index) % LIVE_POINTS];
}

int16_t trendDuty(uint16_t index) {
  return trendValue(0, index);
}

int16_t trendTemp(uint8_t sensor, uint16_t index) {
  return trendValue(sensor == 0 ? 1 : 2, index);
}

}  // namespace session_stats
