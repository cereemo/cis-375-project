#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class UserController : public drogon::HttpController<UserController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UserController::exampleAPI, "/api/example", Post);
    ADD_METHOD_TO(UserController::signup, "/api/signup", Post);
    METHOD_LIST_END

    static void exampleAPI(const HttpRequestPtr &req, std::function<void (const HttpResponsePtr &)> &&callback);
    void signup(const HttpRequestPtr &req, std::function<void (const HttpResponsePtr &)> &&callback);
};
