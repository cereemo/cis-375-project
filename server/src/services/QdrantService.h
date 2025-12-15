#pragma once

#include <drogon/drogon.h>
#include <vector>
#include <string>
#include <unordered_set>

class QdrantService {
public:
    struct ProductVectors {
        std::vector<float> clip;
        std::vector<float> semantic;
        std::vector<float> search;
    };

    static drogon::Task<bool> initCollection() {
        auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:6333");

        Json::Value payload;

        payload["vectors"]["clip"]["size"] = 512;
        payload["vectors"]["clip"]["distance"] = "Cosine";

        payload["vectors"]["semantic"]["size"] = 384;
        payload["vectors"]["semantic"]["distance"] = "Cosine";

        payload["vectors"]["search"]["size"] = 768;
        payload["vectors"]["search"]["distance"] = "Cosine";

        auto request = drogon::HttpRequest::newHttpJsonRequest(payload);
        request->setMethod(drogon::Put);
        request->setPath("/collections/products");

        auto response = co_await client->sendRequestCoro(request);

        if (response->getStatusCode() == 200 || response->getStatusCode() == 409) {
            LOG_INFO << "Qdrant collection 'products' ready with multi-vector support";
            co_return true;
        }

        LOG_ERROR << "Qdrant Init Failed: " << response->getBody();
        co_return false;
    }

    static drogon::Task<bool> upsertProduct(int productId, const ProductVectors& textVectors, const std::vector<std::vector<float>>& imageVectors) {
        auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:6333");

        Json::Value pointsArray(Json::arrayValue);

        Json::Value textPoint;
        textPoint["id"] = productId * 1000;

        Json::Value textVectorObj;

        Json::Value clipArr(Json::arrayValue);
        for (float f : textVectors.clip) clipArr.append(f);
        textVectorObj["clip"] = clipArr;

        Json::Value semArr(Json::arrayValue);
        for (float f : textVectors.semantic) semArr.append(f);
        textVectorObj["semantic"] = semArr;

        Json::Value searchArr(Json::arrayValue);
        for (float f : textVectors.search) searchArr.append(f);
        textVectorObj["search"] = searchArr;

        textPoint["vector"] = textVectorObj;

        Json::Value textPayload;
        textPayload["product_id"] = productId;
        textPayload["vector_type"] = "text";
        textPoint["payload"] = textPayload;

        pointsArray.append(textPoint);

        for (size_t i = 0; i < imageVectors.size(); i++) {
            Json::Value imgPoint;
            imgPoint["id"] = productId * 1000 + static_cast<int>(i) + 1;

            Json::Value imgVectorObj;
            Json::Value imgClipArr(Json::arrayValue);
            for (float f : imageVectors[i]) imgClipArr.append(f);
            imgVectorObj["clip"] = imgClipArr;

            imgPoint["vector"] = imgVectorObj;

            Json::Value imgPayload;
            imgPayload["product_id"] = productId;
            imgPayload["vector_type"] = "image";
            imgPayload["image_index"] = static_cast<int>(i);
            imgPoint["payload"] = imgPayload;

            pointsArray.append(imgPoint);
        }

        Json::Value body;
        body["points"] = pointsArray;

        auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Put);
        request->setPath("/collections/products/points");

        auto response = co_await client->sendRequestCoro(request);

        LOG_INFO << "Upserted product " << productId << " (1 text + " << imageVectors.size() << " images)";

        co_return response->getStatusCode() == 200;
    }

    static drogon::Task<std::vector<int>> search(const ProductVectors& queryVectors, int limit = 20) {
        auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:6333");

        Json::Value prefetchArray(Json::arrayValue);

        if (!queryVectors.clip.empty()) {
            Json::Value clipQuery;
            Json::Value clipVec(Json::arrayValue);
            for (float f : queryVectors.clip) clipVec.append(f);
            clipQuery["using"] = "clip";
            clipQuery["query"] = clipVec;
            clipQuery["limit"] = limit * 3;
            prefetchArray.append(clipQuery);
        }

        if (!queryVectors.semantic.empty()) {
            Json::Value semQuery;
            Json::Value semVec(Json::arrayValue);
            for (float f : queryVectors.semantic) semVec.append(f);
            semQuery["using"] = "semantic";
            semQuery["query"] = semVec;
            semQuery["limit"] = limit * 3;
            prefetchArray.append(semQuery);
        }

        if (!queryVectors.search.empty()) {
            Json::Value searchQuery;
            Json::Value searchVec(Json::arrayValue);
            for (float f : queryVectors.search) searchVec.append(f);
            searchQuery["using"] = "search";
            searchQuery["query"] = searchVec;
            searchQuery["limit"] = limit * 3;
            prefetchArray.append(searchQuery);
        }

        Json::Value body;
        body["prefetch"] = prefetchArray;
        body["query"]["fusion"] = "rrf";
        body["limit"] = limit;
        body["with_payload"] = true;

        auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Post);
        request->setPath("/collections/products/points/query");

        auto response = co_await client->sendRequestCoro(request);

        std::vector<int> productIds;

        if (response->getStatusCode() == 200) {
            auto json = response->getJsonObject();
            if (json && json->isMember("result") && (*json)["result"].isObject() && (*json)["result"].isMember("points") && (*json)["result"]["points"].isArray()) {
                std::unordered_set<int> seen;

                for (const auto& hit : (*json)["result"]["points"]) {
                    if (hit.isMember("payload") &&
                        hit["payload"].isMember("product_id"))
                    {
                        int prodId = hit["payload"]["product_id"].asInt();
                        if (!seen.contains(prodId)) {
                            productIds.push_back(prodId);
                            seen.insert(prodId);
                        }
                    }
                }
            }
        } else {
            LOG_ERROR << "Search failed: " << response->getBody();
        }

        co_return productIds;
    }

    static drogon::Task<int> countPoints() {
        auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:6333");

        auto request = drogon::HttpRequest::newHttpRequest();
        request->setMethod(drogon::Get);
        request->setPath("/collections/products");

        auto response = co_await client->sendRequestCoro(request);
        LOG_INFO << "Collection Stats: " << response->getBody();

        // Parse to get point count
        try {
            auto respBody = response->getJsonObject();
            if (respBody && respBody->isMember("result")) {
                auto result = (*respBody)["result"];
                if (result.isMember("points_count")) {
                    int count = result["points_count"].asInt();
                    LOG_INFO << "Current points count: " << count;
                    co_return count;
                }
            }
        } catch (...) {}

        co_return -1;
    }

    static drogon::Task<bool> deleteProductPoints(int productId) {
        auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:6333");

        Json::Value match;
        match["value"] = productId;

        Json::Value condition;
        condition["key"] = "product_id";
        condition["match"] = match;

        Json::Value must(Json::arrayValue);
        must.append(condition);

        Json::Value filter;
        filter["must"] = must;

        Json::Value body;
        body["filter"] = filter;

        auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Post);
        request->setPath("/collections/products/points/delete");

        auto response = co_await client->sendRequestCoro(request);
        co_return response->getStatusCode() == 200;
    }
};