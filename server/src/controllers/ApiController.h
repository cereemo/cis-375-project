#pragma once
#include <drogon/HttpController.h>

class ApiController : public drogon::HttpController<ApiController> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ApiController::accountCreationCode, "/api/account_creation_code", drogon::Post);
        ADD_METHOD_TO(ApiController::createAccount, "/api/create_account", drogon::Post);
        ADD_METHOD_TO(ApiController::login, "/api/login", drogon::Post);
        ADD_METHOD_TO(ApiController::refreshToken, "/api/refresh", drogon::Post);
    METHOD_LIST_END

    static drogon::Task<drogon::HttpResponsePtr> accountCreationCode(drogon::HttpRequestPtr req);
    static drogon::Task<drogon::HttpResponsePtr> createAccount(drogon::HttpRequestPtr req);
    static drogon::Task<drogon::HttpResponsePtr> login(drogon::HttpRequestPtr req);
    static drogon::Task<drogon::HttpResponsePtr> refreshToken(drogon::HttpRequestPtr req);
};