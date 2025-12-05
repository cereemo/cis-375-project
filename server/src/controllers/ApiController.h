#pragma once
#include <drogon/HttpController.h>

class ApiController : public drogon::HttpController<ApiController> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ApiController::postEndpoint, "/api/data", drogon::Post);
    METHOD_LIST_END

    static void postEndpoint(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};