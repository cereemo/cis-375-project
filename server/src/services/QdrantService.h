#pragma once

#include <drogon/drogon.h>

class QdrantService {
public:
    struct VectorPoint {
        std::vector<float> vector;
        std::string type;
    };

    static drogon::Task<bool> initCollection() {
        auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:6333");

        Json::Value payload;
        payload["vectors"]["size"] = 512;
        payload["vectors"]["distance"] = "Cosine";

        auto request = drogon::HttpRequest::newHttpJsonRequest(payload);
        request->setMethod(drogon::Put);
        request->setPath("/collections/products");

        auto response = co_await client->sendRequestCoro(request);
        if (response->getStatusCode() == 200 || response->getStatusCode() == 409) {
            LOG_INFO << "Qdrant collection 'products' (dim: 512) is ready.";
            co_return true;
        }

        LOG_ERROR << "Qdrant Init Failed: " << response->getBody();
        co_return false;
    }

    static drogon::Task<bool> upsertProductPoints(int productId, const std::vector<VectorPoint>& points) {
        auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:6333");

        Json::Value batch(Json::arrayValue);
        for (const auto& p : points) {
            Json::Value point;
            point["id"] = drogon::utils::getUuid();

            Json::Value payload;
            payload["product_id"] = productId;
            payload["type"] = p.type;
            point["payload"] = payload;

            Json::Value vecArr(Json::arrayValue);
            for (const float f : p.vector) vecArr.append(f);
            point["vector"] = vecArr;

            batch.append(point);
        }

        Json::Value body;
        body["points"] = batch;

        auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Put);
        request->setPath("/collections/products/points");

        auto response = co_await client->sendRequestCoro(request);
        co_return response->getStatusCode() == 200;
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
        const auto client = drogon::HttpClient::newHttpClient("http://host.docker.internal:6333");

        Json::Value body;
        Json::Value vecArr(Json::arrayValue);
        for (const float f : queryVector) vecArr.append(f);

        body["vector"] = vecArr;
        body["limit"] = limit;
        body["with_payload"] = true;

        const auto request = drogon::HttpRequest::newHttpJsonRequest(body);
        request->setMethod(drogon::Post);
        request->setPath("/collections/products/points/search");

        const auto response = co_await client->sendRequestCoro(request);

        std::vector<int> productIds;
        if (response->getStatusCode() == 200) {
            const auto json = response->getJsonObject();
            for (const auto& hit : (*json)["result"]) productIds.push_back(hit["payload"]["product_id"].asInt());
        }

        co_return productIds;
    }
};