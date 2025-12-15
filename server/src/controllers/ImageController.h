#pragma once
#include <drogon/HttpController.h>

using namespace drogon;

class ImageController : public drogon::HttpController<ImageController> {
public:
    METHOD_LIST_BEGIN
        // Maps http://localhost:5050/images/my-shoe.jpg
        ADD_METHOD_TO(ImageController::getImage, "/images/{filename}", Get);
    METHOD_LIST_END

    static void getImage(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback, const std::string &filename);
};