#pragma once

#include <drogon/drogon.h>
#include "services/QdrantService.h"

class MlService {
private:
    static std::optional<QdrantService::VectorPoint> parseVector(const drogon::HttpResponsePtr& response) {
        if (!response || response->getStatusCode() != 200) {
            LOG_ERROR << "ML Service Error: " << (response ? response->getBody() : "no response");
            return std::nullopt;
        }

        auto json = response->getJsonObject();
        if (!json || !json->isMember("vector") || !json->isMember("type")) {
            LOG_ERROR << "ML Service Error: invalid response";
            return std::nullopt;
        }

        QdrantService::VectorPoint point;
        point.type = (*json)["type"].asString();

        for (const auto& v : (*json)["vector"]) {
            point.vector.push_back(v.asFloat());
        }

        return point;
    }

public:
    // Unified text embedding using CLIP
    static drogon::Task<std::optional<QdrantService::VectorPoint>> embedText(const std::string& text) {
        const auto client = drogon::HttpClient::newHttpClient(
            "http://host.docker.internal:5050"
        );

        Json::Value body;
        body["text"] = text;

        const auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Post);
        request->setPath("/embed/text");

        const auto response = co_await client->sendRequestCoro(request);
        co_return parseVector(response);
    }

    // Image embedding using CLIP
    static drogon::Task<std::optional<QdrantService::VectorPoint>> embedImage(const std::string& filename) {
        const auto client = drogon::HttpClient::newHttpClient(
            "http://host.docker.internal:5050"
        );

        Json::Value body;
        body["filename"] = filename;

        const auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Post);
        request->setPath("/embed/image");

        const auto response = co_await client->sendRequestCoro(request);
        co_return parseVector(response);
    }
};