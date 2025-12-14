#include "ProductController.h"
#include "utils/ApiHelpers.h"
#include "services/MlService.h"
#include "services/QdrantService.h"
#include <filesystem>

namespace fs = std::filesystem;

drogon::Task<drogon::HttpResponsePtr> ProductController::addProduct(drogon::HttpRequestPtr req) {
    if (auto userJson = req->attributes()->get<Json::Value>("user"); !userJson.isMember("role") || userJson["role"].asString() != "admin") co_return createResponse({{"error", "Admins Only"}, {"field", "client"}}, drogon::k403Forbidden);

    drogon::MultiPartParser fileParser;
    if (fileParser.parse(req) != 0 || fileParser.getFiles().empty()) co_return createResponse({{"error", "No files uploaded"}, {"field", "client"}}, drogon::k400BadRequest);

    auto params = fileParser.getParameters();
    std::string name = params["name"];
    std::string description = params["description"];
    double price = std::stod(params["price"]);

    auto postgres = drogon::app().getDbClient();
    int productId;

    try {
        auto result = co_await postgres->execSqlCoro("INSERT INTO products (name, description, price) VALUES ($1, $2, $3) RETURNING id", name, description, price);
        productId = result[0]["id"].as<int>();
    } catch (drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "Postgres Error: " << e.base().what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    std::string uploadBase = "/app/uploads/";
    std::string productDir = uploadBase + std::to_string(productId) + "/";

    if (!fs::exists(productDir)) fs::create_directories(productDir);

    Json::Value imageList(Json::arrayValue);
    std::vector<QdrantService::VectorPoint> pointsToUpsert;

    auto descriptionVector = co_await MlService::embedText(description);
    if (!description.empty()) pointsToUpsert.push_back({descriptionVector, "description"});

    auto files = fileParser.getFiles();
    int count = 0;

    for (const auto& file : files) {
        if (count >= 6) break;

        std::string ext = std::string(file.getFileExtension());
        std::string safeName = std::to_string(count) + "." + ext;

        (void) file.saveAs(productDir + safeName);

        std::string relativePath = std::to_string(productId) + "/" + safeName;

        imageList.append(relativePath);

        auto imageVec = co_await MlService::embedImage(relativePath);
        if (!imageVec.empty()) pointsToUpsert.push_back({imageVec, "image"});

        count++;
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string jsonStr = Json::writeString(builder, imageList);

    try {
        co_await postgres->execSqlCoro("UPDATE products SET images=$1::jsonb WHERE id=$2", jsonStr, productId);
    } catch (drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "Postgres Error: " << e.base().what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    co_await QdrantService::upsertProductPoints(productId, pointsToUpsert);

    co_return createResponse({{"id", productId}, {"images", imageList}}, drogon::k201Created);
}

drogon::Task<drogon::HttpResponsePtr> ProductController::deleteProduct(drogon::HttpRequestPtr req, int productId) {
    if (auto userJson = req->attributes()->get<Json::Value>("user"); !userJson.isMember("role") || userJson["role"].asString() != "admin") co_return createResponse({{"error", "Admins Only"}, {"field", "client"}}, drogon::k403Forbidden);

    auto postgres = drogon::app().getDbClient();

    try {
        auto response = co_await postgres->execSqlCoro("DELETE FROM products WHERE id=$1", productId);
        if (response.affectedRows() == 0) co_return createResponse({{"error", "Product not found"}, {"field", "client"}}, drogon::k404NotFound);
    } catch (drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "Postgres Error: " << e.base().what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    bool qSuccess = co_await QdrantService::deleteProductPoints(productId);
    if (!qSuccess) LOG_WARN << "Failed to clean up Qdrant points for product " << productId;

    std::string productDir = "/app/uploads/" + std::to_string(productId);

    try {
        if (fs::exists(productDir)) fs::remove_all(productDir);
    } catch (const std::exception& e) {
        LOG_ERROR << "File cleanup error: " << e.what();
    }

    co_return createResponse({{"id", productId}}, drogon::k200OK);
}

drogon::Task<drogon::HttpResponsePtr> ProductController::searchProduct(drogon::HttpRequestPtr req) {
    std::string query = req->getParameter("q");
    if (query.empty()) co_return createResponse({{"error", "Missing query"}, {"field", "client"}}, drogon::k400BadRequest);

    auto queryVector = co_await MlService::embedText(query);
    auto rawIds = co_await QdrantService::search(queryVector, 20);

    std::vector<int> uniqueIds;
    std::set<int> seen;
    for (int id : rawIds) {
        if (!seen.contains(id)) {
            uniqueIds.push_back(id);
            seen.insert(id);
        }
    }

    if (uniqueIds.empty()) co_return createResponse({{"data", Json::Value(Json::arrayValue)}}, drogon::k200OK);

    std::string sql = "SELECT id, name, price, description, images FROM products WHERE id in (";
    for (size_t i = 0; i < uniqueIds.size(); i++) {
        sql += std::to_string(uniqueIds[i]);
        if (i < uniqueIds.size() - 1) sql += ",";
    }
    sql += ")";

    auto postgres = drogon::app().getDbClient();
    Json::Value list(Json::arrayValue);

    try {
        auto rows = co_await postgres->execSqlCoro(sql);

        for (const auto& row : rows) {
            Json::Value p;
            p["id"] = row["id"].as<int>();
            p["name"] = row["name"].as<std::string>();
            p["price"] = row["price"].as<double>();

            auto imgStr = row["images"].as<std::string>();
            Json::Reader reader;
            Json::Value imagesJson;
            reader.parse(imgStr, imagesJson);
            p["images"] = imagesJson;
            if (!imagesJson.empty()) p["thumbnail"] = imagesJson[0];

            std::string description = row["description"].as<std::string>();
            if (description.length() > 15) {
                p["snippet"] = description.substr(0, 15) + "...";
            } else {
                p["snippet"] = description;
            }

            list.append(p);
        }
    } catch (drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "Postgres Error: " << e.base().what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    co_return createResponse({{"data", list}}, drogon::k200OK);
}
