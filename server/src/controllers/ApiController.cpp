#include "ApiController.h"
#include <json/json.h>

void ApiController::postEndpoint(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    const auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(drogon::k200OK);
    callback(response);
}
