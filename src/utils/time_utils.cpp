#include "time_utils.h"
#include <time.h>

String iso8601Now() {
  time_t now = time(nullptr);
  if (now < 100000) return ""; // Time not set
  struct tm t;
  gmtime_r(&now, &t);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S.000Z", &t);
  return String(buf);
}
