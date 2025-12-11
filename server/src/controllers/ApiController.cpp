#include "ApiController.h"
#include "utils/JWT.h"
#include "utils/JsonParser.h"
#include "utils/Validator.h"
#include "utils/Cryptography.h"
#include "utils/ApiHelpers.h"

drogon::Task<drogon::HttpResponsePtr> ApiController::accountCreationCode(drogon::HttpRequestPtr req) {
    // Check for credentials
    const auto json = req->getJsonObject();
    auto parsedJson = JsonParser(json.get(), {"email"});
    if (!parsedJson) co_return createResponse({{"error", "Invalid request body"}, {"field", "client"}}, drogon::k400BadRequest);
    auto email = parsedJson.value()["email"];
    std::ranges::transform(email, email.begin(), [](const unsigned char c) { return std::tolower(c); });

    // Check credentials formatting
    if (const auto email_validation = validateEmail(email); std::get<0>(email_validation) == false) co_return createResponse({{"error", std::get<1>(email_validation)}, {"field", "email"}}, drogon::k400BadRequest);

    // Get Postgres connection
    const auto postgres = drogon::app().getDbClient();

    // Check for email duplication
    try {
        const drogon::orm::Result result = co_await postgres->execSqlCoro("SELECT EXISTS (SELECT 1 FROM users WHERE email = $1)", email);
        if (result.empty()) co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
        if (result[0][0].as<bool>()) co_return createResponse({{"error", "An account currently using this email already exists"}, {"field", "email"}}, drogon::k400BadRequest);
    } catch (const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "Postgres SQL Error: " << e.base().what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    // Make Code (6-digit)
    const std::string code = generateCode(6);

    // Get Redis connection
    auto redis = drogon::app().getRedisClient();

    // Add code to redis state
    try {
        // Create and get ID field from Redis
        auto id = co_await redis->execCommandCoro("INCR seq:verify_id");
        if (id.type() == drogon::nosql::RedisResultType::kNil) throw std::runtime_error("failed to generate Redis ID");

        long long verifyId = id.asInteger();
        std::string key = std::to_string(verifyId);

        // Use Redis Hash with fields: code, email, attempts
        co_await redis->execCommandCoro("HSET %s code %s attempts 0", key.c_str(), code.c_str(), email.c_str());

        // Set Expiry (15 minutes)
        co_await redis->execCommandCoro("EXPIRE %s 900", key.c_str());

        // Generate JWT
        Json::Value payload;
        payload["vid"] = static_cast<Json::Int64>(verifyId);
        payload["type"] = "creation_code";
        payload["exp"] = time(nullptr) + 900;

        // Sign JWT in Vault
        auto tokenOpt = co_await JwtService::sign(payload);
        if (!tokenOpt) co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);

        // Todo: Send Email

        co_return createResponse({{"token", *tokenOpt}}, drogon::k200OK);
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis/JWT Error: " << e.what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }
}

drogon::Task<drogon::HttpResponsePtr> ApiController::createAccount(drogon::HttpRequestPtr req) {
    // Check for credentials
    const auto json = req->getJsonObject();
    auto parsedJson = JsonParser(json.get(), {"email", "password", "code", "token"});
    if (!parsedJson) co_return createResponse({{"error", "Invalid request body"}, {"field", "client"}}, drogon::k400BadRequest);
    auto email = parsedJson.value()["email"];
    std::ranges::transform(email, email.begin(), [](const unsigned char c) { return std::tolower(c); });
    const auto password = parsedJson.value()["password"];
    const auto code = parsedJson.value()["code"];
    auto token_0 = parsedJson.value()["token"];

    // Check credentials formatting
    if (const auto email_validation = validateEmail(email); std::get<0>(email_validation) == false) co_return createResponse({{"error", std::get<1>(email_validation)}, {"field", "email"}}, drogon::k400BadRequest);
    if (const auto password_validation = validatePassword(password); std::get<0>(password_validation) == false) co_return createResponse({{"error", std::get<1>(password_validation)}, {"field", "password"}}, drogon::k400BadRequest);
    if (const auto code_validation = validateCode(code); std::get<0>(code_validation) == false) co_return createResponse({{"error", std::get<1>(code_validation)}, {"field", "code"}}, drogon::k400BadRequest);

    // Verify/Extract JWT
    auto payload_0 = JwtService::verify(token_0);
    if (!payload_0 || (*payload_0)["vid"].isNull() || (*payload_0)["type"].isNull() || (*payload_0)["exp"].isNull() || (*payload_0)["type"].asString() != "creation_code") co_return createResponse({{"error", "Invalid JWT"}, {"field", "client"}}, drogon::k400BadRequest);
    if (!(*payload_0)["exp"].isInt() || !(*payload_0)["exp"].isInt64() || (*payload_0)["exp"].asInt64() < time(nullptr)) co_return createResponse({{"error", "Expired session, please request another code"}, {"field", "client"}}, drogon::k400BadRequest);
    const auto verifyId = (*payload_0)["vid"].asString();

    // Get Redis connection
    auto redis = drogon::app().getRedisClient();

    // Validate and extract session from Redis
    try {
        auto redis_response = co_await redis->execCommandCoro("HGETALL %s", verifyId.c_str());
        if (redis_response.type() == drogon::nosql::RedisResultType::kNil || redis_response.asArray().empty()) co_return createResponse({{"error", "Expired session, please request another code"}, {"field", "client"}}, drogon::k400BadRequest);
        auto args = redis_response.asArray();
        std::string correct_code;
        int attempts = 0;
        for (size_t i = 0; i < args.size(); i += 2) {
            if (args[i].asString() == "code") correct_code = args[i + 1].asString();
            else if (args[i].asString() == "attempts") attempts = std::stoi(args[i + 1].asString());
        }
        if (attempts >= 5) {
            co_await redis->execCommandCoro("DEL %s", verifyId.c_str());
            co_return createResponse({{"error", "Too many attempts, please request a new code."}, {"field", "code"}}, drogon::k400BadRequest);
        }
        if (correct_code != code) {
            co_await redis->execCommandCoro("HINCRBY %s attempts 1", verifyId.c_str());
            co_return createResponse({{"error", "Incorrect code"}, {"field", "code"}}, drogon::k400BadRequest);
        }
        co_await redis->execCommandCoro("DEL %s", verifyId.c_str());
    } catch (std::exception& e) {
        LOG_ERROR << "Redis Error: " << e.what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    // Create user
    std::string passwordHash = HashMethods::hashPassword(password);
    if (passwordHash.empty()) createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);

    // Get postgres connection
    auto postgres = drogon::app().getDbClient();

    // Add user to postgres
    try {
        co_await postgres->execSqlCoro("INSERT INTO users (email, password_hash, token_version) VALUES ($1, $2, 1)", email, passwordHash);
    } catch (const drogon::orm::DrogonDbException &e) {
        // Check if it's a duplication error
        if (std::string(e.base().what()).starts_with("ERROR:  duplicate key value")) co_return createResponse({{"error", "An account currently using this email already exists"}, {"field", "email"}}, drogon::k400BadRequest);
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    co_return createResponse({{"message", "success"}, {"field", "server"}}, drogon::k201Created);
}

