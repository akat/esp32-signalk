#ifndef SERVICES_ALARMS_H
#define SERVICES_ALARMS_H

#include "../signalk/globals.h"

// Alarm monitoring functions
void updateGeofence();
void updateDepthAlarm(double depth);
void updateWindAlarm(double windSpeedMS);

#endif // SERVICES_ALARMS_H
