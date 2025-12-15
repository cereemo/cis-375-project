#include <drogon/drogon.h>
#include "utils/AppRole.h"
#include <cstdlib>
#include "services/QdrantService.h"
#include "services/VaultService.h"
#include "filters/JwtFilter.h"
#include <filesystem>

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
    VaultManager::init("bf71b960-b0fc-b0f7-852a-e7483131b437", "79034169-e87e-dfcc-851c-99fdf22c8578");

    // (On login) Connect to jwt key
    VaultManager::registerLoginFunction([]() {
        LOG_INFO << "Vault is ready. Starting KeyManager...";
        KeyManager::start("jwt-key");
    });

    // Start background thread (Vault)
    std::thread vaultThread(vaultBackgroundWorker);
    vaultThread.detach();

    drogon::app().getLoop()->queueInLoop([]() {
        drogon::async_run([]() -> drogon::Task<void> {
            bool success = co_await QdrantService::initCollection();
            if (!success) {
                LOG_ERROR << "Qdrant Init failed! Search may be broken.";
            }
        });
    });

    drogon::app().loadConfigFile("./config.json");

    drogon::app().registerPreRoutingAdvice([](const drogon::HttpRequestPtr &req, drogon::FilterCallback &&stop, drogon::FilterChainCallback &&pass) {
        if (req->method() == drogon::HttpMethod::Options) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");

            stop(resp);
            return;
        }
        pass();
    });

    drogon::app().registerPostHandlingAdvice([](const drogon::HttpRequestPtr &, const drogon::HttpResponsePtr &resp) {
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
    });

    LOG_INFO << "Document root: " << drogon::app().getDocumentRoot();

    drogon::app().registerHandler(
    "/images/{productId}/{fileName}",
    [](const drogon::HttpRequestPtr& req,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
           const std::string& productId,
           const std::string& fileName) {
            std::string filePath = "/app/uploads/" + productId + "/" + fileName;
            auto resp = drogon::HttpResponse::newFileResponse(filePath);
            callback(resp);
        },
        {drogon::Get}
    );

    drogon::app().run();
    return 0;
}