#pragma once
#include <drogon/HttpController.h>

class ProductController : public drogon::HttpController<ProductController> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ProductController::addProduct, "/api/products", drogon::Post, "JwtFilter");
        ADD_METHOD_TO(ProductController::deleteProduct, "/api/products/{1}", drogon::Delete, "JwtFilter");
        ADD_METHOD_TO(ProductController::getProduct, "/api/products/{1}", drogon::Get);
        ADD_METHOD_TO(ProductController::searchProduct, "/api/products/search", drogon::Get);
    METHOD_LIST_END

    static drogon::Task<drogon::HttpResponsePtr> addProduct(drogon::HttpRequestPtr req);
    static drogon::Task<drogon::HttpResponsePtr> deleteProduct(drogon::HttpRequestPtr req, int productId);
    static drogon::Task<drogon::HttpResponsePtr> getProduct(drogon::HttpRequestPtr req, int productId);
    static drogon::Task<drogon::HttpResponsePtr> searchProduct(drogon::HttpRequestPtr req);
};