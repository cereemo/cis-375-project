cmake_minimum_required(VERSION 3.20)
project(ecommerce_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src)

file(GLOB_RECURSE SRC_FILES "src/*.cc" "src/*.cpp")

add_executable(ecommerce_server ${SRC_FILES}
        src/controllers/ApiController.h
        src/controllers/ApiController.cpp)

file(COPY ${CMAKE_SOURCE_DIR}/config.json DESTINATION ${CMAKE_BINARY_DIR})

target_link_libraries(ecommerce_server
        PRIVATE
        Drogon::Drogon
)