#pragma once
#include <string>
#include <optional>
#include "crow.h"

// Handlers for account/profile-related endpoints. Each function takes the
// already-authenticated user id (resolved by main.cpp's auth middleware)
// and the parsed request body where relevant, and returns a crow::json::wvalue
// response body. HTTP status is set by the caller in main.cpp based on the
// AccountResult.status field, keeping route registration in one place.

namespace account {

struct Result {
    int status;                 // HTTP status code to return
    crow::json::wvalue body;    // JSON response body
};

Result getAccount(long long userId);
Result getBalance(long long userId);
Result getProfile(long long userId);
Result updateProfile(long long userId, const crow::json::rvalue& body);
Result changePassword(long long userId, const crow::json::rvalue& body);

} // namespace account
