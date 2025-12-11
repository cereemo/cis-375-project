#include "ApiController.h"
#include <json/json.h>
#include "utils/JWT.h"

drogon::Task<drogon::HttpResponsePtr> ApiController::accountCreationCode(const drogon::HttpRequestPtr req) {
    /*
     * Returns a JWT that contains: target_email (string), code (string), attempts (int), expiry (long)
     * JWT is signed with EdDSA (Edwards-curve Digital Signature Algorithm)
     */

    Json::Value payload;
    payload["sub"] = "123abc";

    auto token = co_await JwtService::sign(payload);

    if (!token) {
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(drogon::k500InternalServerError);
        co_return response;
    }

    Json::Value ret;
    ret["token"] = *token;
    co_return drogon::HttpResponse::newHttpJsonResponse(ret);
}
