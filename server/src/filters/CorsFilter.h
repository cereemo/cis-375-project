#pragma once

#include <drogon/HttpFilter.h>

class CorsFilter : public drogon::HttpFilter<CorsFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback &&fcb, drogon::FilterChainCallback&& fccb) override {
        if (req->method() == drogon::Options) {
            auto response = drogon::HttpResponse::newHttpResponse();
            response->addHeader("Access-Control-Allow-Origin", "*");
            response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            response->addHeader("Access-Control-Allow-Headers", "Authorization, Content-Type, X-Requested-With");
            response->setStatusCode(drogon::k200OK);
            fcb(response);
            return;
        }

        fccb();
    }
};