#include "ApiController.h"
#include "utils/JWT.h"
#include "utils/JsonParser.h"
#include "utils/Validator.h"
#include "utils/Cryptography.h"
#include "utils/ApiHelpers.h"
#include "utils/IpHelper.h"

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
        std::string key = "verify:" + std::to_string(verifyId);

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

        LOG_INFO << "Code: " << code;

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
    auto redis_key = "verify:" + verifyId;

    // Validate and extract session from Redis
    try {
        auto redis_response = co_await redis->execCommandCoro("HGETALL %s", redis_key.c_str());
        if (redis_response.type() == drogon::nosql::RedisResultType::kNil || redis_response.asArray().empty()) co_return createResponse({{"error", "Expired session, please request another code"}, {"field", "client"}}, drogon::k400BadRequest);
        auto args = redis_response.asArray();
        std::string correct_code;
        int attempts = 0;
        for (size_t i = 0; i < args.size(); i += 2) {
            if (args[i].asString() == "code") correct_code = args[i + 1].asString();
            else if (args[i].asString() == "attempts") attempts = std::stoi(args[i + 1].asString());
        }
        if (attempts >= 5) {
            co_await redis->execCommandCoro("DEL %s", redis_key.c_str());
            co_return createResponse({{"error", "Too many attempts, please request a new code."}, {"field", "code"}}, drogon::k400BadRequest);
        }
        if (correct_code != code) {
            co_await redis->execCommandCoro("HINCRBY %s attempts 1", redis_key.c_str());
            co_return createResponse({{"error", "Incorrect code"}, {"field", "code"}}, drogon::k400BadRequest);
        }
        co_await redis->execCommandCoro("DEL %s", redis_key.c_str());
    } catch (std::exception& e) {
        LOG_ERROR << "Redis Error: " << e.what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    // Create user
    std::string passwordHash = HashMethods::hashPassword(password);
    if (passwordHash.empty()) co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);

    // Get postgres connection
    auto postgres = drogon::app().getDbClient();
    int user_id;

    // Add user to postgres
    try {
        auto result = co_await postgres->execSqlCoro("INSERT INTO users (email, password_hash, token_version) VALUES ($1, $2, 1) RETURNING id", email, passwordHash);
        user_id = result[0]["id"].as<int>();
    } catch (const drogon::orm::DrogonDbException &e) {
        // Check if it's a duplication error
        LOG_INFO << "Database exception: " << e.base().what();
        if (std::string(e.base().what()).starts_with("ERROR:  duplicate key value")) co_return createResponse({{"error", "An account currently using this email already exists"}, {"field", "email"}}, drogon::k400BadRequest);
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    // Generate token
    ROLE role = isAdmin(email) ? ADMIN : MEMBER;
    auto access_token = co_await createAuthToken(user_id, 1, ACCESS, role);
    auto refresh_token = co_await createAuthToken(user_id, 1, REFRESH, role);
    if (!access_token || !refresh_token) co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);

    co_return createResponse({{"access_token", *access_token}, {"refresh_token", *refresh_token}}, drogon::k201Created);
}

drogon::Task<drogon::HttpResponsePtr> ApiController::login(drogon::HttpRequestPtr req) {
    // Check for credentials
    const auto json = req->getJsonObject();
    auto parsedJson = JsonParser(json.get(), {"email", "password"});
    if (!parsedJson) co_return createResponse({{"error", "Invalid request body"}, {"field", "client"}}, drogon::k400BadRequest);
    auto email = parsedJson.value()["email"];
    std::ranges::transform(email, email.begin(), [](const unsigned char c) { return std::tolower(c); });
    const auto password = parsedJson.value()["password"];

    // Check credentials formatting
    if (const auto email_validation = validateEmail(email); std::get<0>(email_validation) == false) co_return createResponse({{"error", std::get<1>(email_validation)}, {"field", "email"}}, drogon::k400BadRequest);
    if (const auto password_validation = validatePassword(password); std::get<0>(password_validation) == false) co_return createResponse({{"error", std::get<1>(password_validation)}, {"field", "password"}}, drogon::k400BadRequest);

    // Ip Timeouts
    std::string ip = IpHelper::getRemoteIp(req);

    std::string redisKey = "login:" + email + ":" + ip;
    auto redis = drogon::app().getRedisClient();

    long long now = time(nullptr);
    int currentAttempts = 0;
    long long blockUntil = 0;

    try {
        auto redis_res = co_await redis->execCommandCoro("HGETALL %s", redisKey.c_str());
        auto args = redis_res.asArray();

        for (size_t i = 0; i < args.size(); i+= 2) {
            if (args[i].asString() == "attempts") currentAttempts = std::stoi(args[i + 1].asString());
            else if (args[i].asString() == "block_until") blockUntil = std::stoll(args[i + 1].asString());
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis Error " << e.what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    if (now < blockUntil) {
        long remainingMinutes = (blockUntil - now) / 60 + 1;
        co_return createResponse({{"error", std::format("Too many attempts. Login blocked for {} minutes", remainingMinutes)}, {"field", "client"}}, drogon::k429TooManyRequests);
    }

    // Check password
    auto postgres = drogon::app().getDbClient();
    int user_id;
    int token_version;
    std::string password_hash;

    try {
        auto result = co_await postgres->execSqlCoro("SELECT id, password_hash, token_version FROM users WHERE email=$1", email);
        if (result.empty()) co_return createResponse({{"error", "Email is not a registered user. Please check for typos or create an account."}, {"field", "email"}}, drogon::k400BadRequest);
        user_id = result[0]["id"].as<int>();
        token_version = result[0]["token_version"].as<int>();
        password_hash = result[0]["password_hash"].as<std::string>();
    } catch (drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "Postgres Error: " << e.base().what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    if (!HashMethods::verifyHash(password_hash, password)) {
        // Add penalty
        int newAttempts = currentAttempts + 1;
        long penaltySeconds = 0;

        if (newAttempts >= 3) penaltySeconds = std::min((newAttempts - 2) * 300, 3600);

        long long newBlockUntil = now + penaltySeconds;

        try {
            co_await redis->execCommandCoro("HSET %s attempts %d block_until %lld", redisKey.c_str(), newAttempts, newBlockUntil);
            co_await redis->execCommandCoro("EXPIRE %s 86400", redisKey.c_str());
        } catch (const std::exception& e) {
            LOG_ERROR << "Redis Error " << e.what();
            co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
        }

        if (penaltySeconds > 0) co_return createResponse({{"error", std::format("Login failed. You are blocked for {} minutes", std::to_string(penaltySeconds / 60))}, {"field", "client"}}, drogon::k429TooManyRequests);
        co_return createResponse({{"error", "Invalid password"}, {"field", "password"}}, drogon::k401Unauthorized);
    }

    try {
        co_await redis->execCommandCoro("DEL %s", redisKey.c_str());
    } catch (...) {}

    // Issue tokens
    ROLE role = isAdmin(email) ? ADMIN : MEMBER;
    auto access_token = co_await createAuthToken(user_id, token_version, ACCESS, role);
    auto refresh_token = co_await createAuthToken(user_id, token_version, REFRESH, role);
    if (!access_token || !refresh_token) co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);

    co_return createResponse({{"access_token", *access_token}, {"refresh_token", *refresh_token}}, drogon::k200OK);
}

drogon::Task<drogon::HttpResponsePtr> ApiController::logoutAll(drogon::HttpRequestPtr req) {
    auto userJson = req->attributes()->get<Json::Value>("user");
    int userId = userJson["sub"].asInt();

    auto postgres = drogon::app().getDbClient();
    try {
        co_await postgres->execSqlCoro("UPDATE users SET token_version = token_version + 1 WHERE id=$1", userId);
    } catch (const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "Logout Error: " << e.base().what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    co_return createResponse({{"message", "Logged out from all devices"}}, drogon::k200OK);
}

drogon::Task<drogon::HttpResponsePtr> ApiController::refreshToken(drogon::HttpRequestPtr req) {

    const auto json = req->getJsonObject();
    auto parsedJson = JsonParser(json.get(), {"refresh_token"});
    if (!parsedJson) co_return createResponse({{"error", "Invalid request body"}, {"field", "client"}}, drogon::k400BadRequest);
    auto token = parsedJson.value()["refresh_token"];

    auto payload = JwtService::verify(token);
    if (!payload) co_return createResponse({{"error", "Invalid or expired token"}, {"field", "client"}}, drogon::k401Unauthorized);

    if (!payload->isMember("type") || (*payload)["type"].asString() != "refresh") co_return createResponse({{"error", "Invalid token type"}, {"field", "client"}}, drogon::k403Forbidden);

    int id = (*payload)["sub"].asInt();

    std::string jti = (*payload)["jti"].asString();
    long exp = (*payload)["exp"].asInt64();
    long now = time(nullptr);

    auto redis = drogon::app().getRedisClient();
    std::string blacklistKey = "blacklist:" + jti;

    try {
        if (auto redis_result = co_await redis->execCommandCoro("EXISTS %s", blacklistKey.c_str()); redis_result.asInteger() == 1) co_return createResponse({{"error", "Token reuse detected. Please login"}, {"field", "client"}}, drogon::k403Forbidden);
        if (long ttl = exp - now + 10; ttl > 0) co_await redis->execCommandCoro("SET %s 1 EX %d", blacklistKey.c_str(), ttl);
    } catch (std::exception& e) {
        LOG_ERROR << "Redis Error: " << e.what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    const int token_version = (*payload)["ver"].asInt();
    ROLE role = MEMBER;

    const auto postgres = drogon::app().getDbClient();
    try {
        const auto result = co_await postgres->execSqlCoro("SELECT email, token_version FROM users WHERE id=$1", id);
        if (result.empty()) co_return createResponse({{"error", "User not found"}, {"field", "client"}}, drogon::k401Unauthorized);
        if (token_version != result[0]["token_version"].as<int>()) co_return createResponse({{"error", "Session revoked. Please login again."}, {"field", "client"}}, drogon::k401Unauthorized);
        if (isAdmin(result[0]["email"].as<std::string>())) role = ADMIN;
    } catch (const std::exception& e) {
        LOG_ERROR << "Postgres Error: " << e.what();
        co_return createResponse({{"error", "Internal server error"}, {"field", "server"}}, drogon::k500InternalServerError);
    }

    auto access_token = co_await createAuthToken(id, token_version, ACCESS, role);
    auto refresh_token = co_await createAuthToken(id, token_version, REFRESH, role);

    if (!access_token || !refresh_token) co_return createResponse({{"error", "Signing failed"}, {"field", "server"}}, drogon::k500InternalServerError);

    co_return createResponse({{"access_token", *access_token}, {"refresh_token", *refresh_token}}, drogon::k200OK);
}
