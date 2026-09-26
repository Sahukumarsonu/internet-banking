#pragma once
#include "crow.h"

// Deposit, withdrawal, and transfer logic. All money handling lives here,
// server-side — the frontend never sends or trusts a balance value.
namespace transaction {

struct Result {
    int status;
    crow::json::wvalue body;
};

Result deposit(long long userId, const crow::json::rvalue& body);
Result withdraw(long long userId, const crow::json::rvalue& body);
Result transfer(long long userId, const crow::json::rvalue& body);

// Query params: type, from, to, page, pageSize (all optional).
Result listTransactions(long long userId, const crow::query_string& qs);
Result getTransactionById(long long userId, long long transactionId);

} // namespace transaction
