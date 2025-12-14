#include "CartController.h"
#include "utils/ApiHelpers.h"

drogon::Task<drogon::HttpResponsePtr> CartController::addToCart(drogon::HttpRequestPtr req) {
    auto user = req->attributes()->get<Json::Value>("user");
    int userId = user["sub"].asInt();

    auto json = req->getJsonObject();
    if (!json || !json->isMember("product_id")) co_return createResponse({{"error", "Missing product_id"}, {"field", "client"}}, drogon::k400BadRequest);

    int productId = (*json)["product_id"].asInt();
    int quantity = (*json)["quantity"].asInt();
    if (quantity <= 0) quantity = 1;

    auto redis = drogon::app().getRedisClient();
    std::string key = "cart:" + std::to_string(userId);

    try {
        co_await redis->execCommandCoro("HINCRBY %s %d %d", key.c_str(), productId, quantity);
        co_await redis->execCommandCoro("EXPIRE %s 2592000", key.c_str());
        co_return createResponse({{"status", "added"}}, drogon::k200OK);
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis Error " << e.what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }
}

drogon::Task<drogon::HttpResponsePtr> CartController::removeFromCart(drogon::HttpRequestPtr req, int productId) {
    auto user = req->attributes()->get<Json::Value>("user");
    int userId = user["sub"].asInt();

    auto redis = drogon::app().getRedisClient();
    std::string key = "cart:" + std::to_string(userId);

    try {
        co_await redis->execCommandCoro("HDEL %s %d", key.c_str(), productId);
        co_return createResponse({{"status", "removed"}}, drogon::k200OK);
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis Error " << e.what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }
}

drogon::Task<drogon::HttpResponsePtr> CartController::getCart(drogon::HttpRequestPtr req) {
    LOG_INFO << "A";
    auto user = req->attributes()->get<Json::Value>("user");
    int userId = user["sub"].asInt();
    LOG_INFO << "B";
    auto redis = drogon::app().getRedisClient();
    std::string key = "cart:" + std::to_string(userId);
    LOG_INFO << "C";
    std::map<int, int> cartItems; // product id -> quantity
    std::string sqlIds;

    try {
        auto redis_result = co_await redis->execCommandCoro("HGETALL %s", key.c_str());
        LOG_INFO << "D";
        if (redis_result.type() == drogon::nosql::RedisResultType::kNil || redis_result.asArray().empty()) co_return createResponse({{"cart", Json::Value(Json::arrayValue)}, {"total", 0.00}}, drogon::k200OK);
        auto args = redis_result.asArray();
        for (size_t i = 0; i < args.size(); i += 2) {
            if (i + 1 >= args.size()) break;
            int pid = std::stoi(args[i].asString());
            int qty = std::stoi(args[i + 1].asString());
            cartItems[pid] = qty;

            sqlIds += args[i].asString();
            if (i + 2 < args.size()) sqlIds += ",";
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis Error " << e.what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }
    LOG_INFO << "E";

    auto db = drogon::app().getDbClient();
    std::string sql = "SELECT id, name, price, images FROM products WHERE id IN (" + sqlIds + ")";
    LOG_INFO << "F";
    try {
        auto productsRes = co_await db->execSqlCoro(sql);
        LOG_INFO << "G";

        // Merge Data
        Json::Value cartList(Json::arrayValue);
        double totalCartPrice = 0.0;

        for (const auto& row : productsRes) {
            int pid = row["id"].as<int>();
            int qty = cartItems[pid];
            double price = row["price"].as<double>();

            Json::Value item;
            item["id"] = pid;
            item["name"] = row["name"].as<std::string>();
            item["price"] = price;
            item["quantity"] = qty;
            item["total"] = price * qty;

            auto imgStr = row["images"].as<std::string>();
            Json::Reader reader;
            Json::Value imagesJson;
            reader.parse(imgStr, imagesJson);
            if (!imagesJson.empty()) item["thumbnail"] = imagesJson[0];

            cartList.append(item);
            totalCartPrice += (price * qty);
        }

        LOG_INFO << "H";

        co_return createResponse({{"items", cartList}, {"cart_total", totalCartPrice}}, drogon::k200OK);
    } catch (const drogon::orm::DrogonDbException &e) {
        LOG_INFO << "Database exception: " << e.base().what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }
}
