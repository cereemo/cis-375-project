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

inline drogon::Task<std::optional<std::string>> createAuthToken(const int user_id, const int token_version, const AuthTokenType type) {
    Json::Value payload;
    payload["id"] = user_id;
    payload["version"] = token_version;
    payload["type"] = type == ACCESS ? "access" : "refresh";
    payload["exp"] = time(nullptr) + (type == ACCESS ? 900 : 86400);
    auto token = co_await JwtService::sign(payload);
    co_return token;
}
/*
inline drogon::Task<std::optional<Json::Value>> verifyAuthToken() {

}
*/