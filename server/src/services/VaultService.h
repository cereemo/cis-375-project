#pragma once

#include <map>
#include <string>
#include <shared_mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <drogon/drogon.h>
#include "utils/AppRole.h"

class KeyManager {
public:
    static void start(const std::string& vaultKeyname) {
        keyName_ = vaultKeyname;
        performUpdateSync();
    }

    static std::string getPublicKey(int version) {
        std::shared_lock lock(mutex_);
        if (const auto it = publicKeys_.find(version); it != publicKeys_.end()) return it->second;
        return "";
    }

    static int getLatestCachedVersion() {
        std::shared_lock lock(mutex_);
        return latestVersion_;
    }

private:
    static inline std::string keyName_;
    static inline std::map<int, std::string> publicKeys_;
    static inline int latestVersion_ = 0;
    static inline std::shared_mutex mutex_;
    static inline unsigned char oidHeader[] = {0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x03, 0x21, 0x00};

    static time_t timegm(struct tm *tm) {
        #ifdef _WIN32
            return _mkgmtime(tm);
        #else
            return ::timegm(tm);
        #endif
    }

    static void scheduleNextUpdate(const double delaySeconds) {
        const double safeDelay = std::max(1.0, delaySeconds);
        LOG_INFO << "Next key refresh scheduled in " << safeDelay << " seconds.";
        drogon::app().getLoop()->runAfter(safeDelay, []() {
           updateKeys();
        });
    }

    static std::string toPem(const std::string& vaultBase64) {
        const std::string rawBytes = drogon::utils::base64Decode(vaultBase64);
        std::string spkiData;
        spkiData.append(reinterpret_cast<char *>(oidHeader), sizeof(oidHeader));
        spkiData.append(rawBytes);

        const std::string spkiBase64 = drogon::utils::base64Encode(spkiData);

        std::string pem = "-----BEGIN PUBLIC KEY-----\n";
        for (size_t i = 0; i < spkiBase64.length(); i += 64) pem += spkiBase64.substr(i, 64) + "\n";
        pem += "-----END PUBLIC KEY-----";
        return pem;
    }

    static void updateKeys() {
        const auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:8200");
        const auto request = drogon::HttpRequest::newHttpRequest();
        request->setMethod(drogon::Get);
        request->setPath("/v1/transit/keys/" + keyName_);

        std::string token = VaultManager::getClientToken();
        if (token.empty()) {
            LOG_ERROR << "Cannot refresh keys: Vault token is empty.";
            scheduleNextUpdate(5.0);
            return;
        }

        request->addHeader("X-Vault-Token", token);

        client->sendRequest(request, [](const drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
            if (result == drogon::ReqResult::Ok && response->getStatusCode() == 200) {
                if (const auto json = response->getJsonObject(); json && json->isMember("data")) {
                    processKeys((*json)["data"]);
                } else {
                    LOG_ERROR << "Invalid vault response format.";
                    scheduleNextUpdate(10.0);
                }
            } else {
                LOG_ERROR << "Key refresh failed. Status: " << (response ? response->getStatusCode() : 0) << " Body: " << (response ? response->getBody() : "null");
                scheduleNextUpdate(10.0);
            }
        });
    }

    static void processKeys(const Json::Value &data) {
        const int latestFromVault = data["latest_version"].asInt();
        const int minDecryption = data["min_decryption_version"].asInt();
        const long autoRotateSeconds = data["auto_rotate_period"].asInt64();
        const auto& keysObject = data["keys"];

        std::map<int, std::string> newKeys;

        int cutoff = std::max(minDecryption, latestFromVault - 2);
        std::string latestCreation;

        for (const auto &keyId : keysObject.getMemberNames()) {
            int version = std::stoi(keyId);
            if (version >= cutoff) newKeys[version] = toPem(keysObject[keyId]["public_key"].asString());
            if (version == latestFromVault) latestCreation = keysObject[keyId]["creation_time"].asString();
        }

        bool rotated = false;
        {
            std::unique_lock lock(mutex_);
            if (latestFromVault > latestVersion_) rotated = true;
            publicKeys_ = std::move(newKeys);
            latestVersion_ = latestFromVault;
        }

        if (rotated) LOG_INFO << "Key rotation detected! Updated to v" << latestFromVault;

        if (autoRotateSeconds <= 0) {
            scheduleNextUpdate(3600);
            return;
        }

        std::tm tm = {};
        std::istringstream ss(latestCreation);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        const auto creationTime = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::from_time_t(timegm(&tm)));

        const auto nextRotationTime = creationTime + std::chrono::seconds(autoRotateSeconds);
        const auto now = std::chrono::system_clock::now();

        if (const auto diff = std::chrono::duration_cast<std::chrono::seconds>(nextRotationTime - now).count(); diff > 0) {
            scheduleNextUpdate(static_cast<double>(diff) + 5.0);
        } else {
            if (rotated) {
                scheduleNextUpdate(static_cast<double>(autoRotateSeconds) + 5.0);
            } else {
                LOG_WARN << "Expected rotation but Vault is still on v" << latestFromVault << ". Retrying,,,";
                scheduleNextUpdate(2.0);
            }
        }
    }

    static void performUpdateSync() {
        updateKeys();
    }
};