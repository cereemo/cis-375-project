#include "ApiController.h"
#include <json/json.h>

void ApiController::accountCreationCode(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    /*
     * Returns a JWT that contains: target_email (string), code (string), attempts (int), expiry (long)
     * JWT is signed with EdDSA (Edwards-curve Digital Signature Algorithm)
     */
    const auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(drogon::k200OK);
    callback(response);
}
