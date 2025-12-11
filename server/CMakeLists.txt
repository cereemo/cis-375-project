cmake_minimum_required(VERSION 3.20)
project(ecommerce_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(jwt-cpp REQUIRED)
find_library(ARGON2_LIB argon2)

include_directories(${CMAKE_SOURCE_DIR}/src)
file(GLOB_RECURSE SRC_FILES "src/*.cc" "src/*.cpp")

add_executable(ecommerce_server ${SRC_FILES}
        src/services/VaultService.h
        src/utils/JWT.h
        src/utils/Cryptography.h
        src/utils/Math.h
        src/utils/ApiHelpers.h)

file(COPY ${CMAKE_SOURCE_DIR}/config.json DESTINATION ${CMAKE_BINARY_DIR})

target_link_libraries(ecommerce_server
        PRIVATE
        Drogon::Drogon
        jsoncpp_lib
        OpenSSL::SSL
        OpenSSL::Crypto
        jwt-cpp::jwt-cpp
        ${ARGON2_LIB}
)