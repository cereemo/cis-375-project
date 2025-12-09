#pragma once
#include <drogon/HttpController.h>
#include <optional>

inline std::optional<std::unordered_map<std::string, std::string>> JsonParser(const Json::Value *json, const std::vector<std::string> &fields) {
    if (!json) return std::nullopt;

    std::unordered_map<std::string, std::string> out;

    for (const std::string &field : fields) {
        if (!json->isMember(field)) return std::nullopt;
        const auto &value = (*json)[field];
        if (!(value.isString() || value.isNumeric() || value.isBool())) return std::nullopt;
        out[field] = value.asString();
    }

    return out;
}