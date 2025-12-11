cmake_minimum_required(VERSION 3.20)
project(ecommerce_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(jsoncpp REQUIRED)

include(FetchContent)

FetchContent_Declare(
        jwt-cpp
        GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
        GIT_TAG v0.7.0
)

set(JWT_SSL_LIBRARY OpenSSL CACHE STRING "" FORCE)
set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JWT_BUILD TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(jwt-cpp)

include_directories(${CMAKE_SOURCE_DIR}/src)
file(GLOB_RECURSE SRC_FILES "src/*.cc" "src/*.cpp")

add_executable(ecommerce_server ${SRC_FILES}
        src/services/VaultService.h
        src/utils/JWT.h)

file(COPY ${CMAKE_SOURCE_DIR}/config.json DESTINATION ${CMAKE_BINARY_DIR})

target_link_libraries(ecommerce_server
        PRIVATE
        Drogon::Drogon
        jsoncpp_lib
        OpenSSL::SSL
        OpenSSL::Crypto
        jwt-cpp::jwt-cpp
)