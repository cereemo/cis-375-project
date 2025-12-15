#include "ProductController.h"
#include "utils/ApiHelpers.h"
#include "services/MlService.h"
#include "services/QdrantService.h"
#include <filesystem>

namespace fs = std::filesystem;

drogon::Task<drogon::HttpResponsePtr> ProductController::addProduct(drogon::HttpRequestPtr req) {
    if (auto userJson = req->attributes()->get<Json::Value>("user");
        !userJson.isMember("role") ||
        userJson["role"].asString() != "admin") {
        co_return createResponse({{"error", "Admins Only"}, {"field", "client"}}, drogon::k403Forbidden);
    }

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
    } catch (const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "Postgres Error: " << e.base().what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    std::string productDir = "/app/uploads/" + std::to_string(productId) + "/";
    fs::create_directories(productDir);

    Json::Value imageList(Json::arrayValue);
    std::vector<QdrantService::VectorPoint> vectors;

    // Embed text description with CLIP
    if (!description.empty()) {
        // Combine name + description for better semantic understanding
        std::string fullText = name + ". " + description;
        if (auto textVec = co_await MlService::embedText(fullText)) {
            vectors.push_back(*textVec);
        }
    }

    // Embed all images with CLIP
    int count = 0;
    for (const auto& file : fileParser.getFiles()) {
        if (count >= 6) break;

        std::string ext = std::string(file.getFileExtension());
        std::string filename = std::to_string(count) + "." + ext;
        (void) file.saveAs(productDir + filename);

        std::string relativePath = std::to_string(productId) + "/" + filename;

        imageList.append(relativePath);

        if (auto imageVec = co_await MlService::embedImage(relativePath)) {
            vectors.push_back(*imageVec);
        }

        count++;
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string jsonStr = Json::writeString(builder, imageList);

    try {
        co_await postgres->execSqlCoro("UPDATE products SET images=$1::jsonb WHERE id=$2", jsonStr, productId);
    } catch (...) {
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    // Upsert averaged CLIP vector
    if (!vectors.empty()) co_await QdrantService::upsertProductPoints(productId, vectors);

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

drogon::Task<drogon::HttpResponsePtr> ProductController::getProduct(drogon::HttpRequestPtr req, int productId) {
    auto postgres = drogon::app().getDbClient();

    try {
        auto result = co_await postgres->execSqlCoro("SELECT id, name, description, price, images FROM products WHERE id=$1", productId);
        if (result.empty()) co_return createResponse({{"error", "Product not found"}, {"field", "client"}}, drogon::k404NotFound);

        auto row = result[0];
        Json::Value p;
        p["id"] = row["id"].as<int>();
        p["name"] = row["name"].as<std::string>();
        p["description"] = row["description"].as<std::string>();
        p["price"] = row["price"].as<double>();

        auto imgStr = row["images"].as<std::string>();
        Json::Reader reader;
        Json::Value imagesJson;
        reader.parse(imgStr, imagesJson);

        p["images"] = imagesJson;

        co_return createResponse({{"data", p}}, drogon::k200OK);
    } catch (drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "Postgres Error: " << e.base().what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

}

drogon::Task<drogon::HttpResponsePtr> ProductController::searchProduct(drogon::HttpRequestPtr req) {
    std::string query = req->getParameter("q");
    if (query.empty()) co_return createResponse({{"error", "Missing query"}, {"field", "client"}}, drogon::k400BadRequest);

    auto queryVecOpt = co_await MlService::embedText(query);
    if (!queryVecOpt) co_return createResponse({{"data", Json::Value(Json::arrayValue)}}, drogon::k200OK);

    auto productIds = co_await QdrantService::search(queryVecOpt->vector, 20);

    if (productIds.empty()) co_return createResponse({{"data", Json::Value(Json::arrayValue)}}, drogon::k200OK);

    // Build ID list for SQL
    std::string sql = "SELECT id, name, price, description, images FROM products WHERE id IN (";

    for (size_t i = 0; i < productIds.size(); ++i) {
        sql += std::to_string(productIds[i]);
        if (i + 1 < productIds.size()) sql += ",";
    }
    sql += ")";

    auto postgres = drogon::app().getDbClient();
    auto rows = co_await postgres->execSqlCoro(sql);

    // Create a map of id -> product data
    std::unordered_map<int, Json::Value> productMap;

    for (const auto& row : rows) {
        Json::Value p;
        p["id"] = row["id"].as<int>();
        p["name"] = row["name"].as<std::string>();
        p["price"] = row["price"].as<double>();

        Json::Value images;
        Json::Reader reader;
        reader.parse(row["images"].as<std::string>(), images);

        p["images"] = images;
        if (!images.empty()) p["thumbnail"] = images[0];

        std::string desc = row["description"].as<std::string>();
        p["snippet"] = desc.length() > 60 ? desc.substr(0, 60) + "..." : desc;

        productMap[row["id"].as<int>()] = p;
    }

    // Build result list in Qdrant's order
    Json::Value list(Json::arrayValue);
    for (int id : productIds) {
        if (productMap.find(id) != productMap.end()) {
            list.append(productMap[id]);
        }
    }

    co_return createResponse({{"data", list}}, drogon::k200OK);
}

drogon::Task<drogon::HttpResponsePtr> ProductController::getFeatured(drogon::HttpRequestPtr req) {
    auto db = drogon::app().getDbClient();

    try {
        auto rows = co_await db->execSqlCoro("SELECT id, name, price, description, images FROM products ORDER BY RANDOM() LIMIT 12");
        Json::Value list(Json::arrayValue);

        for (const auto& row : rows) {
            Json::Value p;
            p["id"] = row["id"].as<int>();
            p["name"] = row["name"].as<std::string>();
            p["price"] = row["price"].as<double>();

            std::string desc = row["description"].as<std::string>();
            if (desc.length() > 60) {
                p["snippet"] = desc.substr(0, 60) + "...";
            } else {
                p["snippet"] = desc;
            }

            std::string imgJsonStr = row["images"].as<std::string>();
            Json::Value images;
            Json::Reader reader;

            if (reader.parse(imgJsonStr, images) && images.isArray() && !images.empty()) {
                p["thumbnail"] = images[0];
            } else {
                p["thumbnail"] = "";
            }

            list.append(p);
        }

        co_return createResponse({{"products", list}}, drogon::k200OK);
    } catch (const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "Postgres Error: " << e.base().what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }
}
