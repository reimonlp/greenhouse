#include "power_loss.h"

static const char* NS_SYSTEM = "system";

void handleStartupPowerLoss(IPersistence& store, IEventLogger& logger) {
    if (!store.begin(NS_SYSTEM, false)) return;
    bool flag = store.getBool("power_lost", false);
    if (flag) {
        logger.logSystemEvent("power_loss_detected", "Previous power loss detected");
        // reset to false before marking active
        store.setBool("power_lost", false);
    }
    // Mark current run as active (set true so if we crash it remains true)
    store.setBool("power_lost", true);
    store.end();
}

void markCleanShutdown(IPersistence& store) {
    if (!store.begin(NS_SYSTEM, false)) return;
    store.setBool("power_lost", false);
    store.end();
}
