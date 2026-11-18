#pragma once
#include <ArduinoJson.h>

// Filtre de flux partagé pour /v1/stationboard : ne garde que les champs
// réellement consommés par ApiClient, afin de borner la taille du document
// parsé sans dépendre d'un buffer intermédiaire (contrainte 0 PSRAM).
JsonDocument buildStationboardFilter();
