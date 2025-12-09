#pragma once
#include <drogon/HttpClient.h>
#include <drogon/drogon.h>
#include <trantor/net/EventLoop.h>
#include <string>
#include <vector>
#include <functional>
#include <mutex>

const std::string VAULT_ADDR = "http://host.docker.internal:8200";

struct VaultToken {
    std::string client_token;
    long lease_duration = 0;
    bool renewable = false;
    std::string role_id;
    std::string secret_id;
};

// Prevent deadlocks
class VaultManager {
private:
    static inline VaultToken tokenData_;
    static inline std::mutex mutex_;
    static inline std::vector<std::function<void()>> loginCallbacks_;
public:
    static inline bool firstLogin_ = true;

    static std::string getClientToken() {
        std::lock_guard lock(mutex_);
        return tokenData_.client_token;
    }

    static void updateToken(const std::string& token, const long ttl, const bool renewable) {
        std::lock_guard lock(mutex_);
        tokenData_.client_token = token;
        tokenData_.lease_duration = ttl;
        tokenData_.renewable = renewable;
    }

    static long getLeaseDuration() {
        std::lock_guard lock(mutex_);
        return tokenData_.lease_duration;
    }

    static std::pair<std::string, std::string> getCredentials() {
        std::lock_guard lock(mutex_);
        return {tokenData_.role_id, tokenData_.secret_id};
    }

    static void init(const std::string& role_id, const std::string& secret_id) {
        std::lock_guard lock(mutex_);
        tokenData_.role_id = role_id;
        tokenData_.secret_id = secret_id;
    }

    static void registerLoginFunction(const std::function<void()> &function) {
        std::lock_guard lock(mutex_);
        loginCallbacks_.push_back(function);
    }

    static void executeLoginFunctions() {
        std::vector<std::function<void()>> targetFunctions;

        {
            std::lock_guard lock(mutex_);
            if (!firstLogin_) return;
            targetFunctions = std::move(loginCallbacks_);
            firstLogin_ = false;
        }

        for (const auto& function : targetFunctions) {
            if (function) function();
        }
    }
};

// Global Variables
// inline VaultToken global_vaultToken("bcb72ec0-d8ae-4a6f-01d9-bc9120c87c2d", "6a2b7229-2ab2-248f-62ec-701d6d3f3565");
// inline std::mutex tokenMutex;

inline bool appRoleLogin() {
    trantor::EventLoop loop;
    auto client = drogon::HttpClient::newHttpClient(VAULT_ADDR);

    Json::Value login_payload;
    std::pair<std::string, std::string> token_credentials = VaultManager::getCredentials();
    login_payload["role_id"] = token_credentials.first;
    login_payload["secret_id"] = token_credentials.second;

    auto request = drogon::HttpRequest::newHttpJsonRequest(login_payload);
    request->setMethod(drogon::Post);
    request->setPath("/v1/auth/approle/login");

    bool success = false;

    client->sendRequest(request, [&](const drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
        if (result == drogon::ReqResult::Ok && response->getStatusCode() == 200) {
            auto json = response->getJsonObject();
            if (json && json->isMember("auth")) {
                const auto& auth = (*json)["auth"];
                VaultManager::updateToken(auth["client_token"].asString(), auth["lease_duration"].asInt64(), auth["renewable"].asBool());

                success = true;
                LOG_INFO << "Vault AppRole login successful. New TTL: " << VaultManager::getLeaseDuration() << "s";

                VaultManager::executeLoginFunctions();
            } else {
                LOG_ERROR << "Vault login failed: Response missing 'auth' object.";
            }
        } else {
            LOG_ERROR << "Vault login failed. Status: " << (response ? std::to_string(response->getStatusCode()) : "N/A");
        }

        loop.quit();
    });

    loop.loop();
    return success;
}

inline bool appRoleRenew() {
    trantor::EventLoop loop;
    const auto client = drogon::HttpClient::newHttpClient(VAULT_ADDR, &loop);

    const auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::HttpMethod::Post);
    request->setPath("/v1/auth/token/renew-self");
    /*
    {
        std::lock_guard lock(tokenMutex);
        request->addHeader("X-Vault-Token", token.client_token);
    }
    */
    request->addHeader("X-Vault-Token", VaultManager::getClientToken());
    request->setBody("{}");
    request->addHeader("Content-Type", "application/json");

    bool success = false;

    client->sendRequest(request, [&](const drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
        if (result == drogon::ReqResult::Ok && response->getStatusCode() == 200) {
            auto json = response->getJsonObject();
            if (json && json->isMember("auth")) {
                const auto& auth = (*json)["auth"];
                VaultManager::updateToken(auth["client_token"].asString(), auth["lease_duration"].asInt64(), auth["renewable"].asBool());

                success = true;
                LOG_INFO << "Vault Token successfully renewed. New TTL: " << VaultManager::getLeaseDuration() << "s";
            } else {
                LOG_ERROR << "Vault renewal failed: Response missing 'auth' object.";
            }
        } else {
            LOG_WARN << "Vault renewal failed. Status: " << (response ? std::to_string(response->getStatusCode()) : "N/A");
        }

        loop.quit();
    });

    loop.loop();

    if (success) return true;

    LOG_WARN << "Renewal failed. Attempting full AppRole login...";
    return appRoleLogin();
}

