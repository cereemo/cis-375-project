#pragma once

#include <drogon/drogon.h>
#include <jwt-cpp/jwt.h>
#include <string>
#include <optional>
#include "utils/AppRole.h"
#include "services/VaultService.h"

class JwtService {
public:
    static drogon::Task<std::optional<std::string>> sign(const Json::Value& payload) {
        const int version = KeyManager::getLatestCachedVersion() * 1;
        if (version <= 0) {
            LOG_ERROR << "Cannot sign JWT: No active keys.";
            co_return std::nullopt;
        }

        Json::Value header;
        header["alg"] = "EdDSA";
        header["typ"] = "JWT";
        header["kid"] = std::to_string(version);

        std::string headerStr = toBase64Url(jsonToString(header));
        std::string payloadStr = toBase64Url(jsonToString(payload));
        std::string signingInput = headerStr + "." + payloadStr;
        std::string vaultInput = drogon::utils::base64Encode(signingInput);

        auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:8200");
        Json::Value vaultBody;
        vaultBody["input"] = vaultInput;
        vaultBody["key_version"] = version;

        auto request = drogon::HttpRequest::newHttpJsonRequest(vaultBody);
        request->setMethod(drogon::Post);
        request->setPath("/v1/transit/sign/jwt-key");
        request->addHeader("X-Vault-Token", VaultManager::getClientToken());

        try {
            const auto response = co_await client->sendRequestCoro(request);

            if (response->getStatusCode() == 200) {
                if (const auto json = response->getJsonObject(); json && json->isMember("data")) {
                    const std::string rawSignature = (*json)["data"]["signature"].asString();
                    co_return assembleJwt(signingInput, rawSignature);
                }
            }

            LOG_ERROR << "Vault Signing Failed!";
            LOG_ERROR << "Status: " << (response ? response->getStatusCode() : 0);
            LOG_ERROR << "Body: " << (response ? response->getBody() : "No response");
        } catch (const std::exception& e) {
            LOG_ERROR << "Vault Network Exception: " << e.what();
        }

        co_return std::nullopt;
    }

    static std::optional<Json::Value> verify(const std::string& token) {
        try {
            auto decoded = jwt::decode(token);

            std::string kid = decoded.get_key_id();
            if (kid.empty()) return std::nullopt;

            int version = std::stoi(kid);

            std::string publicKey = KeyManager::getPublicKey(version);
            if (publicKey.empty()) {
                LOG_WARN << "JWT verification failed: Unknown key version " << version;
                return std::nullopt;
            }

            auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::ed25519(publicKey)).leeway(5);
            verifier.verify(decoded);

            const std::string& payloadString = decoded.get_payload();

            Json::Value payloadJson;
            const Json::CharReaderBuilder builder;
            std::string errs;

            if (const std::unique_ptr<Json::CharReader> reader(builder.newCharReader()); !reader->parse(payloadString.c_str(), payloadString.c_str() + payloadString.size(), &payloadJson, &errs)) {
                LOG_ERROR << "Failed to parse JWT payload JSON: " << errs;
                return std::nullopt;
            }

            return payloadJson;
        } catch (const std::exception& e) {
            LOG_WARN << "JWT Verify Error: " << e.what();
            return std::nullopt;
        }
    }
private:
    static std::string jsonToString(const Json::Value& json) {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        return Json::writeString(builder, json);
    }

    static std::string toBase64Url(const std::string& input) {
        std::string b64 = drogon::utils::base64Encode(input);
        b64.erase(std::remove(b64.begin(), b64.end(), '='), b64.end());
        std::replace(b64.begin(), b64.end(), '+', '=');
        std::replace(b64.begin(), b64.end(), '/', '_');
        return b64;
    }

    static std::string assembleJwt(const std::string& signingInput, const std::string& vaultSignature) {
        size_t lastColon = vaultSignature.rfind(':');
        if (lastColon == std::string::npos) return "";

        std::string rawB64Signature = vaultSignature.substr(lastColon + 1);

        std::string urlSafeSignature = rawB64Signature;
        urlSafeSignature.erase(std::remove(urlSafeSignature.begin(), urlSafeSignature.end(), '='), urlSafeSignature.end());
        std::replace(urlSafeSignature.begin(), urlSafeSignature.end(), '+', '-');
        std::replace(urlSafeSignature.begin(), urlSafeSignature.end(), '/', '_');

        return signingInput + "." + urlSafeSignature;
    }
};