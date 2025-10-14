#ifndef POWER_LOSS_H
#define POWER_LOSS_H

#include "persistence.h"

// Event logger abstraction (minimal) so we can test without full database module
class IEventLogger {
public:
    virtual void logSystemEvent(const char* type, const char* message) = 0;
    virtual ~IEventLogger() {}
};

// Detect previous unclean shutdown (power loss) using persistence flag "power_lost".
// Semantics:
//  - On startup: if flag == true => log event, then overwrite to false, then mark current run active (true)
//  - On startup: if flag == false => simply mark current run active (true)
//  - Clean shutdown (optional) should call markCleanShutdown() to set flag false.
void handleStartupPowerLoss(IPersistence& store, IEventLogger& logger);
void markCleanShutdown(IPersistence& store);

#endif // POWER_LOSS_H
