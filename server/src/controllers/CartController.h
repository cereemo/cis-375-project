#pragma once

#include <drogon/HttpController.h>

class CartController : public drogon::HttpController<CartController>
{
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(CartController::getCart, "/api/cart", drogon::Get, "JwtFilter");
        ADD_METHOD_TO(CartController::addToCart, "/api/cart", drogon::Post, "JwtFilter");
        ADD_METHOD_TO(CartController::removeFromCart, "/api/cart/{1}", drogon::Delete, "JwtFilter");
    METHOD_LIST_END

    static drogon::Task<drogon::HttpResponsePtr> getCart(drogon::HttpRequestPtr req);
    static drogon::Task<drogon::HttpResponsePtr> addToCart(drogon::HttpRequestPtr req);
    static drogon::Task<drogon::HttpResponsePtr> removeFromCart(drogon::HttpRequestPtr req, int productId);
};