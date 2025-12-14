#pragma once

#include <drogon/drogon.h>

class MlService {
private:
    static std::vector<float> parseVector(const drogon::HttpResponsePtr& response) {
        std::vector<float> vec;
        if (response->getStatusCode() != 200) {
            LOG_ERROR << "ML Service Error: " << response->getBody();
            return vec;
        }

        auto json = response->getJsonObject();
        if (json && json->isMember("vector")) {
            for (const auto& val : (*json)["vector"]) vec.push_back(val.asFloat());
        }

        return vec;
    }
public:
    static drogon::Task<std::vector<float>> embedText(const std::string& text) {
        const auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:5050");

        Json::Value body;
        body["text"] = text;

        const auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Post);
        request->setPath("/embed/text");

        const auto response = co_await client->sendRequestCoro(request);
        co_return parseVector(response);
    }

    static drogon::Task<std::vector<float>> embedImage(const std::string& filename) {
        const auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:5050");

        Json::Value body;
        body["filename"] = filename;
        auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Post);
        request->setPath("/embed/image");

        auto response = co_await client->sendRequestCoro(request);
        co_return parseVector(response);
    }
};