#pragma once

#include <functional>
#include <regex>
#include <unordered_map>
#include <format>

/* Main Data Validator */
inline std::tuple<bool, std::string> validateData(std::unordered_map<std::string, std::string> &data, const std::unordered_map<std::string, std::function<std::tuple<bool, std::string>(const std::string&)>> &validators, bool break_early = true) {
    bool isValid = true;
    std::string messages;

    for (const auto& [key, value] : data) {
        if (!validators.contains(key)) throw std::runtime_error(std::format("Missing validator key for {}", key));
        if (std::tuple<bool, std::string> validator_result = validators.at(key)(value); !std::get<0>(validator_result)) {
            messages += std::format("{}{}", messages.empty() ? "" : ";", std::get<1>(validator_result));
            isValid = false;
            if (break_early) break;
        }
    }

    return std::make_tuple(isValid, messages);
}

/* Validators for Main Data Validator in form (string) -> tuple(bool, string) */
inline std::tuple<bool, std::string> validateEmail(const std::string &email) {
    const static std::regex email_regex(R"(^(([^<>()\[\]\\.,;:\s@"]+(\.[^<>()\[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$)");

    if (email.empty()) return std::make_tuple(false, "Email required");
    if (email.length() > 320) return std::make_tuple(false, "Email exceeded the maximum limit of 320 characters");

    if (!std::regex_match(email, email_regex)) return std::make_tuple(false, "Invalid email format");

    return std::make_tuple(true, "");
}

inline std::tuple<bool, std::string> validatePassword(const std::string &password) {
    if (password.length() < 8) return std::make_tuple(false, "Password requires a minimum of 8 characters");
    if (password.length() > 32) return std::make_tuple(false, "Password exceeded the maximum limit of 32 characters");

    int lowercase = 0, uppercase = 0, digit = 0, miscellaneous  = 0;
    for (const char character : password) {
        if (islower(character)) lowercase++;
        else if (isupper(character)) uppercase++;
        else if (isdigit(character)) digit++;
        else miscellaneous++;
    }

    if (lowercase < 1) return std::make_tuple(false, "Password requires a minimum of 1 lowercase characters");
    if (uppercase < 1) return std::make_tuple(false, "Password requires a minimum of 1 uppercase characters");
    if (digit < 2) return std::make_tuple(false, "Password requires a minimum of 2 digits");
    if (miscellaneous < 1) return std::make_tuple(false, "Password requires a minimum of 1 miscellaneous characters");

    return std::make_tuple(true, "");
}

inline std::tuple<bool, std::string> validateCode(const std::string &code, const int length = 6, const bool digits_only = true) {
    const static std::regex digits_regex(R"(^\d+$)");

    if (code.length() != length) std::make_tuple(false, "Code must contain 6 characters");

    if (digits_only && !std::regex_match(code, digits_regex)) std::make_tuple(false, "Code must only contain digits");

    return std::make_tuple(true, "");
}