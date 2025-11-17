#ifndef SERVICES_DYNDNS_H
#define SERVICES_DYNDNS_H

// Initialize internal state for DynDNS updater
void initDynDnsService();

// Run periodic DynDNS update checks (call from loop)
void processDynDnsService();

// Request an immediate DynDNS update attempt
void requestDynDnsUpdate();

#endif  // SERVICES_DYNDNS_H
