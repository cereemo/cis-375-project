#pragma once

#include <json/json.h>
#include <initializer_list>
#include <optional>
#include "utils/JWT.h"

inline drogon::HttpResponsePtr createResponse(const std::initializer_list<std::pair<std::string, Json::Value>>& data, const drogon::HttpStatusCode statusCode) {
    /*
     * Forms a JSON response from a map and adds a status to it
     */
    Json::Value response_data;
    for (const auto&[k, v] : data) response_data[k] = v;
    drogon::HttpResponsePtr response = drogon::HttpResponse::newHttpJsonResponse(response_data);
    response->setStatusCode(statusCode);
    return response;
}

enum AuthTokenType { ACCESS, REFRESH };
enum ROLE { MEMBER, ADMIN };

inline drogon::Task<std::optional<std::string>> createAuthToken(const int user_id, const int token_version, const AuthTokenType type, const ROLE role = MEMBER) {
    Json::Value payload;
    payload["sub"] = user_id;
    payload["ver"] = token_version;
    payload["jti"] = drogon::utils::getUuid();
    payload["type"] = type == ACCESS ? "access" : "refresh";
    payload["role"] = role == MEMBER ? "member" : "admin";
    payload["exp"] = time(nullptr) + (type == ACCESS ? 900 : 86400);
    auto token = co_await JwtService::sign(payload);
    co_return token;
}
/*
inline drogon::Task<std::optional<Json::Value>> verifyAuthToken() {

}
*/
inline bool isAdmin(const std::string& email) {
    auto config = drogon::app().getCustomConfig();
    if (config.isMember("admin_emails") && config["admin_emails"].isArray()) {
        for (const auto& adminEmail : config["admin_emails"]) {
            if (adminEmail.asString() == email) {
                return true;
            }
        }
    }

    return false;
}