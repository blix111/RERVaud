#include "json_filter.h"

JsonDocument buildStationboardFilter() {
  JsonDocument filter;
  JsonObject stationboard = filter["stationboard"].add<JsonObject>();
  stationboard["stop"]["departure"] = true;
  stationboard["stop"]["delay"] = true;
  stationboard["category"] = true;
  stationboard["number"] = true;
  stationboard["to"] = true;
  return filter;
}
