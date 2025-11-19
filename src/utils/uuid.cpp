#include "uuid.h"
#include <esp_random.h>

String generateUUID() {
  char uuid[57];
  snprintf(uuid, sizeof(uuid), "urn:mrn:signalk:uuid:%08x-%04x-%04x-%04x-%012x",
    esp_random(), esp_random() & 0xFFFF, esp_random() & 0xFFFF,
    esp_random() & 0xFFFF, esp_random());
  return String(uuid);
}
