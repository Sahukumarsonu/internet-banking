#include "transaction.h"
#include "database.h"
#include <stdexcept>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <string>

namespace {

constexpr long long MAX_AMOUNT_PAISE = 10'000'000'00LL; // 1 crore INR safety ceiling for a demo

// Converts a JSON number (which may arrive as a double like 500.50) to
// integer paise. Rejects anything that isn't cleanly representable to
// avoid silent rounding of money.
bool amountToPaise(const crow::json::rvalue& val, long long& outPaise) {
    if (val.t() != crow::json::type::Number) return false;
    double rupees = val.d();
    if (rupees <= 0) return false;
    double paiseD = rupees * 100.0;
    long long paise = static_cast<long long>(paiseD + (paiseD >= 0 ? 0.5 : -0.5));
    // Reject if rounding moved us more than half a paisa away (i.e. bad input like 3 decimal places)
    if (std::abs(paiseD - static_cast<double>(paise)) > 0.5) return false;
    if (paise <= 0 || paise > MAX_AMOUNT_PAISE) return false;
    outPaise = paise;
    return true;
}

struct AccountRow {
    long long id;
    std::string accountNumber;
    long long balancePaise;
};

bool getAccountByUserId(sqlite3* handle, long long userId, AccountRow& out) {
    Statement stmt(handle, "SELECT id, account_number, balance_paise FROM accounts WHERE user_id = ?");
    stmt.bindInt(1, userId);
    if (!stmt.step()) return false;
    out.id = stmt.columnInt(0);
    out.accountNumber = stmt.columnText(1);
    out.balancePaise = stmt.columnInt(2);
    return true;
}

bool getAccountByNumber(sqlite3* handle, const std::string& accNumber, AccountRow& out) {
    Statement stmt(handle, "SELECT id, account_number, balance_paise FROM accounts WHERE account_number = ?");
    stmt.bindText(1, accNumber);
    if (!stmt.step()) return false;
    out.id = stmt.columnInt(0);
    out.accountNumber = stmt.columnText(1);
    out.balancePaise = stmt.columnInt(2);
    return true;
}

} // namespace

namespace transaction {

Result deposit(long long userId, const crow::json::rvalue& body) {
    if (!body.has("amount")) {
        crow::json::wvalue err; err["error"] = "amount is required"; return {400, std::move(err)};
    }
    long long amountPaise;
    if (!amountToPaise(body["amount"], amountPaise)) {
        crow::json::wvalue err; err["error"] = "Invalid deposit amount"; return {400, std::move(err)};
    }
    std::string description = body.has("description") ? std::string(body["description"].s()) : "Cash deposit";

    auto& db = Database::instance();
    std::lock_guard<std::mutex> lock(db.writeMutex());

    AccountRow acc;
    if (!getAccountByUserId(db.handle(), userId, acc)) {
        crow::json::wvalue err; err["error"] = "Account not found"; return {404, std::move(err)};
    }

    if (!db.begin()) {
        crow::json::wvalue err; err["error"] = "Could not start transaction"; return {500, std::move(err)};
    }

    Statement upd(db.handle(), "UPDATE accounts SET balance_paise = balance_paise + ? WHERE id = ?");
    upd.bindInt(1, amountPaise);
    upd.bindInt(2, acc.id);
    if (!upd.execute()) {
        db.rollback();
        crow::json::wvalue err; err["error"] = "Deposit failed"; return {500, std::move(err)};
    }

    Statement ins(db.handle(),
        "INSERT INTO transactions (type, amount_paise, account_id, description, status) "
        "VALUES ('deposit', ?, ?, ?, 'completed')");
    ins.bindInt(1, amountPaise);
    ins.bindInt(2, acc.id);
    ins.bindText(3, description);
    if (!ins.execute()) {
        db.rollback();
        crow::json::wvalue err; err["error"] = "Deposit failed"; return {500, std::move(err)};
    }

    if (!db.commit()) {
        db.rollback();
        crow::json::wvalue err; err["error"] = "Deposit failed"; return {500, std::move(err)};
    }

    crow::json::wvalue res;
    res["message"] = "Deposit successful";
    res["newBalance"] = (acc.balancePaise + amountPaise) / 100.0;
    return {200, std::move(res)};
}

Result withdraw(long long userId, const crow::json::rvalue& body) {
    if (!body.has("amount")) {
        crow::json::wvalue err; err["error"] = "amount is required"; return {400, std::move(err)};
    }
    long long amountPaise;
    if (!amountToPaise(body["amount"], amountPaise)) {
        crow::json::wvalue err; err["error"] = "Invalid withdrawal amount"; return {400, std::move(err)};
    }
    std::string description = body.has("description") ? std::string(body["description"].s()) : "Cash withdrawal";

    auto& db = Database::instance();
    std::lock_guard<std::mutex> lock(db.writeMutex());

    AccountRow acc;
    if (!getAccountByUserId(db.handle(), userId, acc)) {
        crow::json::wvalue err; err["error"] = "Account not found"; return {404, std::move(err)};
    }

    if (!db.begin()) {
        crow::json::wvalue err; err["error"] = "Could not start transaction"; return {500, std::move(err)};
    }

    // The WHERE clause re-checks the balance atomically at the DB level,
    // so a race between two concurrent withdrawals can never overdraw.
    Statement upd(db.handle(),
        "UPDATE accounts SET balance_paise = balance_paise - ? WHERE id = ? AND balance_paise >= ?");
    upd.bindInt(1, amountPaise);
    upd.bindInt(2, acc.id);
    upd.bindInt(3, amountPaise);
    upd.execute();

    if (sqlite3_changes(db.handle()) == 0) {
        db.rollback();
        crow::json::wvalue err; err["error"] = "Insufficient balance"; return {400, std::move(err)};
    }

    Statement ins(db.handle(),
        "INSERT INTO transactions (type, amount_paise, account_id, description, status) "
        "VALUES ('withdrawal', ?, ?, ?, 'completed')");
    ins.bindInt(1, amountPaise);
    ins.bindInt(2, acc.id);
    ins.bindText(3, description);
    if (!ins.execute()) {
        db.rollback();
        crow::json::wvalue err; err["error"] = "Withdrawal failed"; return {500, std::move(err)};
    }

    if (!db.commit()) {
        db.rollback();
        crow::json::wvalue err; err["error"] = "Withdrawal failed"; return {500, std::move(err)};
    }

    crow::json::wvalue res;
    res["message"] = "Withdrawal successful";
    res["newBalance"] = (acc.balancePaise - amountPaise) / 100.0;
    return {200, std::move(res)};
}

Result transfer(long long userId, const crow::json::rvalue& body) {
    if (!body.has("receiverAccountNumber") || !body.has("amount")) {
        crow::json::wvalue err; err["error"] = "receiverAccountNumber and amount are required"; return {400, std::move(err)};
    }
    long long amountPaise;
    if (!amountToPaise(body["amount"], amountPaise)) {
        crow::json::wvalue err; err["error"] = "Invalid transfer amount"; return {400, std::move(err)};
    }
    std::string receiverAccNumber = body["receiverAccountNumber"].s();
    std::string description = body.has("description") ? std::string(body["description"].s()) : "Fund transfer";

    auto& db = Database::instance();
    std::lock_guard<std::mutex> lock(db.writeMutex());

    AccountRow sender;
    if (!getAccountByUserId(db.handle(), userId, sender)) {
        crow::json::wvalue err; err["error"] = "Sender account not found"; return {404, std::move(err)};
    }

    AccountRow receiver;
    if (!getAccountByNumber(db.handle(), receiverAccNumber, receiver)) {
        crow::json::wvalue err; err["error"] = "Receiver account does not exist"; return {400, std::move(err)};
    }

    if (receiver.id == sender.id) {
        crow::json::wvalue err; err["error"] = "Cannot transfer to your own account"; return {400, std::move(err)};
    }

    if (!db.begin()) {
        crow::json::wvalue err; err["error"] = "Could not start transaction"; return {500, std::move(err)};
    }

    // Deduct from sender, guarded by balance check at the DB level.
    Statement deduct(db.handle(),
        "UPDATE accounts SET balance_paise = balance_paise - ? WHERE id = ? AND balance_paise >= ?");
    deduct.bindInt(1, amountPaise);
    deduct.bindInt(2, sender.id);
    deduct.bindInt(3, amountPaise);
    deduct.execute();

    if (sqlite3_changes(db.handle()) == 0) {
        db.rollback();
        crow::json::wvalue err; err["error"] = "Insufficient balance"; return {400, std::move(err)};
    }

    Statement credit(db.handle(), "UPDATE accounts SET balance_paise = balance_paise + ? WHERE id = ?");
    credit.bindInt(1, amountPaise);
    credit.bindInt(2, receiver.id);
    if (!credit.execute()) {
        db.rollback();
        crow::json::wvalue err; err["error"] = "Transfer failed"; return {500, std::move(err)};
    }

    Statement outRow(db.handle(),
        "INSERT INTO transactions (type, amount_paise, account_id, counterparty_account_id, description, status) "
        "VALUES ('transfer_out', ?, ?, ?, ?, 'completed')");
    outRow.bindInt(1, amountPaise);
    outRow.bindInt(2, sender.id);
    outRow.bindInt(3, receiver.id);
    outRow.bindText(4, description);
    if (!outRow.execute()) {
        db.rollback();
        crow::json::wvalue err; err["error"] = "Transfer failed"; return {500, std::move(err)};
    }

    Statement inRow(db.handle(),
        "INSERT INTO transactions (type, amount_paise, account_id, counterparty_account_id, description, status) "
        "VALUES ('transfer_in', ?, ?, ?, ?, 'completed')");
    inRow.bindInt(1, amountPaise);
    inRow.bindInt(2, receiver.id);
    inRow.bindInt(3, sender.id);
    inRow.bindText(4, description);
    if (!inRow.execute()) {
        db.rollback();
        crow::json::wvalue err; err["error"] = "Transfer failed"; return {500, std::move(err)};
    }

    if (!db.commit()) {
        db.rollback();
        crow::json::wvalue err; err["error"] = "Transfer failed"; return {500, std::move(err)};
    }

    crow::json::wvalue res;
    res["message"] = "Transfer successful";
    res["newBalance"] = (sender.balancePaise - amountPaise) / 100.0;
    res["receiverAccountNumber"] = receiver.accountNumber;
    return {200, std::move(res)};
}

Result listTransactions(long long userId, const crow::query_string& qs) {
    auto& db = Database::instance();

    AccountRow acc;
    if (!getAccountByUserId(db.handle(), userId, acc)) {
        crow::json::wvalue err; err["error"] = "Account not found"; return {404, std::move(err)};
    }

    std::string typeFilter = qs.get("type") ? std::string(qs.get("type")) : "";
    std::string fromDate = qs.get("from") ? std::string(qs.get("from")) : "";
    std::string toDate = qs.get("to") ? std::string(qs.get("to")) : "";
    int page = qs.get("page") ? std::max(1, std::atoi(qs.get("page"))) : 1;
    int pageSize = qs.get("pageSize") ? std::atoi(qs.get("pageSize")) : 20;
    if (pageSize <= 0 || pageSize > 100) pageSize = 20;
    int offset = (page - 1) * pageSize;

    std::string sql =
        "SELECT id, type, amount_paise, description, status, created_at, counterparty_account_id "
        "FROM transactions WHERE account_id = ?";
    if (!typeFilter.empty()) sql += " AND type = ?";
    if (!fromDate.empty()) sql += " AND date(created_at) >= date(?)";
    if (!toDate.empty()) sql += " AND date(created_at) <= date(?)";
    sql += " ORDER BY created_at DESC, id DESC LIMIT ? OFFSET ?";

    Statement stmt(db.handle(), sql);
    int idx = 1;
    stmt.bindInt(idx++, acc.id);
    if (!typeFilter.empty()) stmt.bindText(idx++, typeFilter);
    if (!fromDate.empty()) stmt.bindText(idx++, fromDate);
    if (!toDate.empty()) stmt.bindText(idx++, toDate);
    stmt.bindInt(idx++, pageSize);
    stmt.bindInt(idx++, offset);

    std::vector<crow::json::wvalue> items;
    while (stmt.step()) {
        crow::json::wvalue row;
        row["id"] = stmt.columnInt(0);
        row["type"] = stmt.columnText(1);
        row["amount"] = stmt.columnInt(2) / 100.0;
        row["description"] = stmt.columnText(3);
        row["status"] = stmt.columnText(4);
        row["createdAt"] = stmt.columnText(5);

        if (!stmt.isColumnNull(6)) {
            long long counterpartyId = stmt.columnInt(6);
            Statement cp(db.handle(), "SELECT account_number FROM accounts WHERE id = ?");
            cp.bindInt(1, counterpartyId);
            if (cp.step()) row["counterpartyAccount"] = cp.columnText(0);
        }
        items.push_back(std::move(row));
    }

    // Total count for pagination (same filters, no limit/offset).
    std::string countSql = "SELECT COUNT(*) FROM transactions WHERE account_id = ?";
    if (!typeFilter.empty()) countSql += " AND type = ?";
    if (!fromDate.empty()) countSql += " AND date(created_at) >= date(?)";
    if (!toDate.empty()) countSql += " AND date(created_at) <= date(?)";
    Statement countStmt(db.handle(), countSql);
    idx = 1;
    countStmt.bindInt(idx++, acc.id);
    if (!typeFilter.empty()) countStmt.bindText(idx++, typeFilter);
    if (!fromDate.empty()) countStmt.bindText(idx++, fromDate);
    if (!toDate.empty()) countStmt.bindText(idx++, toDate);
    countStmt.step();
    long long total = countStmt.columnInt(0);

    crow::json::wvalue res;
    res["items"] = std::move(items);
    res["page"] = page;
    res["pageSize"] = pageSize;
    res["total"] = total;
    return {200, std::move(res)};
}

Result getTransactionById(long long userId, long long transactionId) {
    auto& db = Database::instance();

    AccountRow acc;
    if (!getAccountByUserId(db.handle(), userId, acc)) {
        crow::json::wvalue err; err["error"] = "Account not found"; return {404, std::move(err)};
    }

    Statement stmt(db.handle(),
        "SELECT id, type, amount_paise, description, status, created_at, counterparty_account_id "
        "FROM transactions WHERE id = ? AND account_id = ?");
    stmt.bindInt(1, transactionId);
    stmt.bindInt(2, acc.id);

    if (!stmt.step()) {
        crow::json::wvalue err; err["error"] = "Transaction not found"; return {404, std::move(err)};
    }

    crow::json::wvalue res;
    res["id"] = stmt.columnInt(0);
    res["type"] = stmt.columnText(1);
    res["amount"] = stmt.columnInt(2) / 100.0;
    res["description"] = stmt.columnText(3);
    res["status"] = stmt.columnText(4);
    res["createdAt"] = stmt.columnText(5);
    if (!stmt.isColumnNull(6)) {
        Statement cp(db.handle(), "SELECT account_number FROM accounts WHERE id = ?");
        cp.bindInt(1, stmt.columnInt(6));
        if (cp.step()) res["counterpartyAccount"] = cp.columnText(0);
    }
    return {200, std::move(res)};
}

} // namespace transaction
