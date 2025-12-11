#pragma once

#include <drogon/drogon.h>
#include <json/json.h>
#include <initializer_list>

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