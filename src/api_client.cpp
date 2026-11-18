#include "api_client.h"

namespace ApiClient {
  bool fetchStationboard(const char* stationApiName,
                          StationboardEntry out[], uint8_t maxOut, uint8_t* outCount) {
    (void)stationApiName; (void)out; (void)maxOut;
    // Phase 2
    *outCount = 0;
    return false;
  }
}
