#include <drogon/drogon.h>
#include "utils/AppRole.h"
#include <cstdlib>
#include <utils/Math.h>

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
    LOG_INFO << IntegerPow(5, 4);

    // Initialize (Vault)
    VaultManager::init("bf71b960-b0fc-b0f7-852a-e7483131b437", "7878cb61-1611-958c-f909-7255b1010c54");

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