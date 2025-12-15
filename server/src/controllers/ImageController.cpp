#include "ImageController.h"
#include <filesystem>

void ImageController::getImage(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback, const std::string &filename) {

    const std::string folderPath = "/app/uploads/";
    const std::string fullPath = folderPath + filename;

    if (filename.find("..") != std::string::npos) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    if (!std::filesystem::exists(fullPath)) {
        LOG_ERROR << "File not found: " << fullPath;
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k404NotFound);
        callback(resp);
        return;
    }

    auto resp = HttpResponse::newFileResponse(fullPath, "", CT_IMAGE_JPG); 
    
    resp->addHeader("Access-Control-Allow-Origin", "*");
    
    callback(resp);
}