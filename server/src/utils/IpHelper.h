#pragma once

#include <drogon/drogon.h>

class IpHelper {
public:
    static std::string getRemoteIp(const drogon::HttpRequestPtr& req) {
        std::string ip = req->getHeader("X-Forwarded-For");
        if (!ip.empty()) {
            size_t comma = ip.find(',');
            if (comma != std::string::npos) return ip.substr(0, comma);
            return ip;
        }

        ip = req->getHeader("X-Real-IP");
        if (!ip.empty()) return ip;

        return req->getPeerAddr().toIp();
    }
};