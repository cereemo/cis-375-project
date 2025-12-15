#pragma once

#include <drogon/drogon.h>
#include <cmath>

class QdrantService {
public:
    struct VectorPoint {
        std::vector<float> vector;
        std::string type;
    };

    static drogon::Task<bool> initCollection() {
        auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:6333");

        Json::Value payload;

        payload["vectors"]["clip"]["size"] = 512;
        payload["vectors"]["clip"]["distance"] = "Cosine";

        auto request = drogon::HttpRequest::newHttpJsonRequest(payload);
        request->setMethod(drogon::Put);
        request->setPath("/collections/products");

        auto response = co_await client->sendRequestCoro(request);

        if (response->getStatusCode() == 200 || response->getStatusCode() == 409) {
            LOG_INFO << "Qdrant collection 'products' is ready (CLIP unified). Body " << response->body();
            co_return true;
        }

        LOG_ERROR << "Qdrant Init Failed: " << response->getBody();
        co_return false;
    }


    static drogon::Task<bool> upsertProductPoints(int productId, const std::vector<VectorPoint>& points) {
        if (points.empty()) co_return false;

        auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:6333");

        Json::Value pointsArray(Json::arrayValue);

        // Create separate point for each vector (text + each image)
        int vectorId = 0;
        for (const auto& p : points) {
            if (p.vector.size() != 512) continue;

            Json::Value vecArr(Json::arrayValue);
            for (float f : p.vector) vecArr.append(f);

            Json::Value vectors(Json::objectValue);
            vectors["clip"] = vecArr;

            Json::Value point(Json::objectValue);
            // Unique ID: productId * 100 + vectorId (e.g., product 5 → IDs 500, 501, 502...)
            point["id"] = productId * 100 + vectorId;
            point["vector"] = vectors;

            Json::Value payload(Json::objectValue);
            payload["product_id"] = productId;  // Store original product ID
            payload["vector_type"] = p.type;     // "text" or "image"
            payload["vector_index"] = vectorId;  // Which image/text this is
            point["payload"] = payload;

            pointsArray.append(point);
            vectorId++;
        }

        Json::Value body(Json::objectValue);
        body["points"] = pointsArray;

        auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Put);
        request->setPath("/collections/products/points");

        auto response = co_await client->sendRequestCoro(request);

        LOG_INFO << "Upserted product " << productId << " with " << points.size()
                 << " separate CLIP vectors";

        co_return response->getStatusCode() == 200;
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

    static drogon::Task<std::vector<int>> search(const std::vector<float>& queryVector, int limit = 20) {
        auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:6333");

        // Verify vector size
        if (queryVector.size() != 512) {
            LOG_ERROR << "Invalid query vector size: " << queryVector.size() << " (expected 512)";
            co_return std::vector<int>{};
        }

        LOG_INFO << "CLIP query vector size: " << queryVector.size();

        // Build query vector
        Json::Value vecArr(Json::arrayValue);
        for (float f : queryVector) vecArr.append(f);

        // Simple query body - no fusion needed
        Json::Value body(Json::objectValue);
        body["vector"] = Json::Value(Json::objectValue);
        body["vector"]["name"] = "clip";
        body["vector"]["vector"] = vecArr;
        body["limit"] = limit;
        body["with_payload"] = true;

        auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Post);
        request->setPath("/collections/products/points/search");
        request->addHeader("Content-Type", "application/json");

        auto response = co_await client->sendRequestCoro(request);

        std::vector<int> productIds;

        if (response->getStatusCode() == 200) {
            auto json = response->getJsonObject();
            if (json && json->isMember("result")) {
                if (const auto& result = (*json)["result"]; result.isArray()) {
                    std::unordered_set<int> seenProducts;
                    for (const auto& hit : result) {
                        if (hit.isMember("payload") && hit["payload"].isMember("product_id")) {
                            int prodId = hit["payload"]["product_id"].asInt();

                            // Only add each product once (first/best match)
                            if (!seenProducts.contains(prodId)) {
                                productIds.push_back(prodId);
                                seenProducts.insert(prodId);

                                if (productIds.size() >= static_cast<size_t>(limit)) break;
                            }
                        }
                    }
                }
            }
        }

        co_return productIds;
    }
};