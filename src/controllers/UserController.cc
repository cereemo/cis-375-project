#include "UserController.h"
#include <json/json.h>
#include "utils/JsonParser.h"
#include "utils/Validator.h"

void UserController::exampleAPI(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto db = drogon::app().getDbClient();

    auto json = req->getJsonObject();
    if (!json) {
        auto res = HttpResponse::newHttpJsonResponse("Invalid JSON");
        res->setStatusCode(k400BadRequest);
        callback(res);
        return;
    }

    if (!json->isMember("name") || !json->isMember("email")) {
        Json::Value resp;
        resp["error"] = "Missing fields 'name' or 'email'";
        auto res = HttpResponse::newHttpJsonResponse(resp);
        res->setStatusCode(k400BadRequest);
        callback(res);
        return;
    }

    std::string name = (*json)["name"].asString();
    std::string email = (*json)["email"].asString();

    *db << "INSERT INTO users (name, email) VALUES ($1, $2) RETURNING id" << name << email
    >> [callback](const orm::Result &res) {
        Json::Value out;
        out["id"] = res[0]["id"].as<int>();
        out["status"] = "ok";
        auto response = HttpResponse::newHttpJsonResponse(out);
        callback(response);
    }
    >> [callback](const drogon::orm::DrogonDbException &e) {
        Json::Value err;
        err["error"] = e.base().what();
        auto response = HttpResponse::newHttpJsonResponse(err);
        response->setStatusCode(k500InternalServerError);
        callback(response);
    };
}

void UserController::signup(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback)
{
    // Parse JSON
    std::optional<std::unordered_map<std::string, std::string>> json = JsonParser(req->getJsonObject().get(), {"email", "password", "code"});
    if (!json.has_value()) {
        Json::Value response_data;
        response_data["error"] = "Invalid JSON format or missing fields";
        const HttpResponsePtr response = HttpResponse::newHttpJsonResponse(response_data);
        response->setStatusCode(k400BadRequest);
        callback(response);
        return;
    }

    // Extract Data
    std::unordered_map<std::string, std::string> data = json.value();
    //std::string email =

    // Validate fields
    if (std::tuple<bool, std::string> validationResults = validateData(data, {"email", validateEmail, "password", validatePassword, "code", validateCode}); !std::get<0>(validationResults)) {
        Json::Value response_data;
        response_data["error"] = std::get<1>(validationResults);
        const HttpResponsePtr response = HttpResponse::newHttpJsonResponse(response_data);
        response->setStatusCode(k400BadRequest);
        callback(response);
        return;
    }

    // Get database connection
    auto db = drogon::app().getDbClient();

    // Duplicate email check
    *db << "SELECT EXISTS (SELECT 1 FROM users WHERE email = $1)" <<
}