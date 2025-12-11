#pragma once

#include <random>
#include <argon2.h>
#include <string>
#include <vector>
#include <random>
#include <cstring>
#include "utils/Math.h"

static std::string generateCode(const int digits) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, IntegerPow(10, digits) - 1);

    std::string code = std::to_string(dis(gen));
    while (code.length() < 6) code.insert(0, "0");
    return code;
}

class HashMethods {
public:
    static constexpr uint32_t T_COST = 4;
    static constexpr uint32_t M_COST = 16;
    static constexpr uint32_t PARALLELISM = 2;
    static constexpr size_t SALT_LEN = 16;
    static constexpr size_t HASH_LEN = 32;

    static std::string hashPassword(const std::string& password) {
        uint8_t salt[SALT_LEN];
        try {
            std::random_device rd;
            std::generate_n(salt, SALT_LEN, std::ref(rd));
        } catch (...) {
            return "";
        }

        const size_t encodedLen = argon2_encodedlen(T_COST, M_COST, PARALLELISM, SALT_LEN, HASH_LEN, Argon2_id);
        std::vector<char> encoded(encodedLen);

        if (const int result = argon2id_hash_encoded(T_COST, M_COST, PARALLELISM, password.c_str(), password.length(), salt, SALT_LEN, HASH_LEN, encoded.data(), encodedLen); result != ARGON2_OK) {
            LOG_ERROR << "Argon2 Hash Error: " << argon2_error_message(result);
            return "";
        }

        return encoded.data();
    }

    static bool verifyHash(const std::string& hash, const std::string& password) {
        const int result = argon2id_verify(hash.c_str(), password.c_str(), password.length());
        return result == ARGON2_OK;
    }
};