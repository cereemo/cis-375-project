#pragma once

#include <drogon/HttpFilter.h>

#include "utils/ApiHelpers.h"
#include "utils/JWT.h"
#include <format>

class JwtFilter : public drogon::HttpFilter<JwtFilter> {
public:
    JwtFilter() {}

    void doFilter(const drogon::HttpRequestPtr &req, drogon::FilterCallback &&fcb, drogon::FilterChainCallback &&fccb) override {
        std::string token = req->getHeader("Authorization");

        if (token.find("Bearer ") == 0) token = token.substr(7);

        if (token.empty()) {
            auto response = drogon::HttpResponse::newHttpResponse();
            response->setStatusCode(drogon::k401Unauthorized);
            response->setBody("Missing Authorization Header");
            fcb(response);
            return;
        }

        if (auto payload = JwtService::verify(token)) {
            req->attributes()->insert("user", *payload);
            fccb();
        } else {
            fcb(createResponse({{"error", "Invalid or expired token"}, {"field", "client"}}, drogon::k401Unauthorized));
        }
    }
};