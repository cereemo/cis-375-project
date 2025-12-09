#include <drogon/drogon.h>
#include "utils/AppRole.h"
#include <cstdlib>

#include "services/VaultService.h"

[[noreturn]] void vaultBackgroundWorker() {
    LOG_INFO << "Vault background worker started.";

    if (!appRoleLogin()) {
        LOG_FATAL << "Could not perform initial login to Vault!";
        exit(1);
    }

    while (true) {
        long sleepSeconds = std::max(5L, VaultManager::getLeaseDuration() * 4 / 5);

        LOG_INFO << "Vault worker sleeping for " << sleepSeconds << "s";
        std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));

        appRoleRenew();
    }
}

int main() {
    // Initialize (Vault)
    VaultManager::init("ccbe696b-0486-7032-8957-a5122fb469f4", "f32feaff-04ee-4b1b-72e3-6b45c60a38b9");

    // (On login) Connect to jwt key
    VaultManager::registerLoginFunction([]() {
        LOG_INFO << "Vault is ready. Starting KeyManager...";
        KeyManager::start("jwt-key");
    });

    // Start background thread (Vault)
    std::thread vaultThread(vaultBackgroundWorker);
    vaultThread.detach();

    drogon::app().loadConfigFile("./config.json");
    drogon::app().run();
    return 0;
}