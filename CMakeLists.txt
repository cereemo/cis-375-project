cmake_minimum_required(VERSION 3.20)
project(ecommerce_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon REQUIRED)
find_package(PostgreSQL REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${PostgreSQL_INCLUDE_DIRS})

file(GLOB_RECURSE SRC_FILES "src/*.cc" "src/*.cpp")

add_executable(ecommerce_server ${SRC_FILES}
        src/utils/jsonParser.h
        src/utils/Validator.h)

file(COPY ${CMAKE_SOURCE_DIR}/config.json DESTINATION ${CMAKE_BINARY_DIR})

target_link_libraries(ecommerce_server
        PRIVATE
        Drogon::Drogon
        PostgreSQL::PostgreSQL
)
