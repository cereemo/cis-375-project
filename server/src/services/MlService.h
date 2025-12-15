#pragma once

#include <drogon/drogon.h>
#include <optional>
#include <vector>

class MlService {
public:
    struct EmbeddingResult {
        std::vector<float> vector;
        std::string model;
        int dimensions;
    };

    struct MultiEmbedding {
        EmbeddingResult clip;
        EmbeddingResult semantic;
        EmbeddingResult search;
    };

private:
    static constexpr const char* ML_SERVICE_URL = "http://host.docker.internal:5050";

    static std::optional<EmbeddingResult> parseEmbedding(const drogon::HttpResponsePtr& response) {
        if (!response || response->getStatusCode() != 200) {
            LOG_ERROR << "ML Service Error: " << (response ? response->getBody() : "no response");
            return std::nullopt;
        }

        auto json = response->getJsonObject();
        if (!json || !json->isMember("vector")) {
            LOG_ERROR << "ML Service Error: invalid response format";
            return std::nullopt;
        }

        EmbeddingResult result;
        result.model = json->isMember("model") ? (*json)["model"].asString() : "unknown";
        result.dimensions = json->isMember("dimensions") ? (*json)["dimensions"].asInt() : 0;

        for (const auto& v : (*json)["vector"]) {
            result.vector.push_back(v.asFloat());
        }

        return result;
    }

    static std::optional<MultiEmbedding> parseMultiEmbedding(const drogon::HttpResponsePtr& response) {
        if (!response || response->getStatusCode() != 200) {
            LOG_ERROR << "ML Service Error: " << (response ? response->getBody() : "no response");
            return std::nullopt;
        }

        auto json = response->getJsonObject();
        if (!json) return std::nullopt;

        MultiEmbedding result;

        if (json->isMember("clip")) {
            const auto& clipJson = (*json)["clip"];
            result.clip.model = "clip";
            result.clip.dimensions = clipJson["dimensions"].asInt();
            for (const auto& v : clipJson["vector"]) {
                result.clip.vector.push_back(v.asFloat());
            }
        }

        if (json->isMember("semantic")) {
            const auto& semJson = (*json)["semantic"];
            result.semantic.model = "semantic";
            result.semantic.dimensions = semJson["dimensions"].asInt();
            for (const auto& v : semJson["vector"]) {
                result.semantic.vector.push_back(v.asFloat());
            }
        }

        if (json->isMember("search")) {
            const auto& searchJson = (*json)["search"];
            result.search.model = "search";
            result.search.dimensions = searchJson["dimensions"].asInt();
            for (const auto& v : searchJson["vector"]) {
                result.search.vector.push_back(v.asFloat());
            }
        }

        return result;
    }

public:
    static drogon::Task<std::optional<MultiEmbedding>> embedTextAll(const std::string& text) {
        const auto client = drogon::HttpClient::newHttpClient(ML_SERVICE_URL);

        Json::Value body;
        body["text"] = text;

        const auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Post);
        request->setPath("/embed/text/all");

        const auto response = co_await client->sendRequestCoro(request);
        co_return parseMultiEmbedding(response);
    }

    static drogon::Task<std::optional<EmbeddingResult>> embedTextClip(const std::string& text, bool addPrefix = false) {
        const auto client = drogon::HttpClient::newHttpClient(ML_SERVICE_URL);

        Json::Value body;
        body["text"] = text;
        body["add_prefix"] = addPrefix;

        const auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Post);
        request->setPath("/embed/text/clip");

        const auto response = co_await client->sendRequestCoro(request);
        co_return parseEmbedding(response);
    }

    static drogon::Task<std::optional<EmbeddingResult>> embedTextSemantic(const std::string& text) {
        const auto client = drogon::HttpClient::newHttpClient(ML_SERVICE_URL);

        Json::Value body;
        body["text"] = text;

        const auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Post);
        request->setPath("/embed/text/semantic");

        const auto response = co_await client->sendRequestCoro(request);
        co_return parseEmbedding(response);
    }

    static drogon::Task<std::optional<EmbeddingResult>> embedTextSearch(const std::string& text) {
        const auto client = drogon::HttpClient::newHttpClient(ML_SERVICE_URL);

        Json::Value body;
        body["text"] = text;

        const auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Post);
        request->setPath("/embed/text/search");

        const auto response = co_await client->sendRequestCoro(request);
        co_return parseEmbedding(response);
    }

    static drogon::Task<std::optional<EmbeddingResult>> embedImage(const std::string& filename) {
        const auto client = drogon::HttpClient::newHttpClient(ML_SERVICE_URL);

        Json::Value body;
        body["filename"] = filename;

        const auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Post);
        request->setPath("/embed/image");

        const auto response = co_await client->sendRequestCoro(request);
        co_return parseEmbedding(response);
    }
};