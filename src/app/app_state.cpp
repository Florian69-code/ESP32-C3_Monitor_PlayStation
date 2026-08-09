#include "app_state.h"

namespace {
AppState gAppState;
}

AppState& getAppState() {
  return gAppState;
}

void resetHistoryBuffer() {
  for (uint8_t i = 0; i < GRAPH_POINTS; ++i) {
    getAppState().dutyHistory[i] = -1.0f;
  }
  getAppState().dutyMax = 0.0f;
}
