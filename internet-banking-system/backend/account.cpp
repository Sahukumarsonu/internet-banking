#include "account.h"
#include "database.h"
#include "auth.h"

namespace account {

Result getAccount(long long userId) {
    auto& db = Database::instance();
    Statement stmt(db.handle(),
        "SELECT account_number, balance_paise, created_at FROM accounts WHERE user_id = ?");
    stmt.bindInt(1, userId);

    if (!stmt.step()) {
        crow::json::wvalue err;
        err["error"] = "Account not found";
        return {404, std::move(err)};
    }

    crow::json::wvalue res;
    res["accountNumber"] = stmt.columnText(0);
    res["balancePaise"] = stmt.columnInt(1);
    res["balance"] = stmt.columnInt(1) / 100.0;
    res["createdAt"] = stmt.columnText(2);
    return {200, std::move(res)};
}

Result getBalance(long long userId) {
    auto& db = Database::instance();
    Statement stmt(db.handle(), "SELECT balance_paise FROM accounts WHERE user_id = ?");
    stmt.bindInt(1, userId);
    if (!stmt.step()) {
        crow::json::wvalue err;
        err["error"] = "Account not found";
        return {404, std::move(err)};
    }
    crow::json::wvalue res;
    res["balancePaise"] = stmt.columnInt(0);
    res["balance"] = stmt.columnInt(0) / 100.0;
    return {200, std::move(res)};
}

Result getProfile(long long userId) {
    auto& db = Database::instance();
    Statement stmt(db.handle(),
        "SELECT u.full_name, u.email, u.phone, u.created_at, a.account_number "
        "FROM users u JOIN accounts a ON a.user_id = u.id WHERE u.id = ?");
    stmt.bindInt(1, userId);
    if (!stmt.step()) {
        crow::json::wvalue err;
        err["error"] = "User not found";
        return {404, std::move(err)};
    }
    crow::json::wvalue res;
    res["fullName"] = stmt.columnText(0);
    res["email"] = stmt.columnText(1);
    res["phone"] = stmt.columnText(2);
    res["createdAt"] = stmt.columnText(3);
    res["accountNumber"] = stmt.columnText(4);
    return {200, std::move(res)};
}

Result updateProfile(long long userId, const crow::json::rvalue& body) {
    if (!body.has("fullName") || !body.has("phone")) {
        crow::json::wvalue err;
        err["error"] = "fullName and phone are required";
        return {400, std::move(err)};
    }

    std::string fullName = body["fullName"].s();
    std::string phone = body["phone"].s();

    if (fullName.empty() || fullName.size() > 100) {
        crow::json::wvalue err; err["error"] = "Invalid full name"; return {400, std::move(err)};
    }
    if (!auth::isValidPhone(phone)) {
        crow::json::wvalue err; err["error"] = "Invalid phone number"; return {400, std::move(err)};
    }

    auto& db = Database::instance();
    std::lock_guard<std::mutex> lock(db.writeMutex());
    Statement stmt(db.handle(),
        "UPDATE users SET full_name = ?, phone = ?, updated_at = datetime('now') WHERE id = ?");
    stmt.bindText(1, fullName);
    stmt.bindText(2, phone);
    stmt.bindInt(3, userId);
    stmt.execute();

    crow::json::wvalue res;
    res["message"] = "Profile updated";
    return {200, std::move(res)};
}

Result changePassword(long long userId, const crow::json::rvalue& body) {
    if (!body.has("currentPassword") || !body.has("newPassword")) {
        crow::json::wvalue err;
        err["error"] = "currentPassword and newPassword are required";
        return {400, std::move(err)};
    }

    std::string currentPassword = body["currentPassword"].s();
    std::string newPassword = body["newPassword"].s();

    if (!auth::isStrongEnoughPassword(newPassword)) {
        crow::json::wvalue err;
        err["error"] = "New password must be at least 8 characters";
        return {400, std::move(err)};
    }

    auto& db = Database::instance();

    std::string storedHash;
    {
        Statement stmt(db.handle(), "SELECT password_hash FROM users WHERE id = ?");
        stmt.bindInt(1, userId);
        if (!stmt.step()) {
            crow::json::wvalue err; err["error"] = "User not found"; return {404, std::move(err)};
        }
        storedHash = stmt.columnText(0);
    }

    if (!auth::verifyPassword(currentPassword, storedHash)) {
        crow::json::wvalue err;
        err["error"] = "Current password is incorrect";
        return {401, std::move(err)};
    }

    std::string newHash = auth::hashPassword(newPassword);
    {
        std::lock_guard<std::mutex> lock(db.writeMutex());
        Statement stmt(db.handle(),
            "UPDATE users SET password_hash = ?, updated_at = datetime('now') WHERE id = ?");
        stmt.bindText(1, newHash);
        stmt.bindInt(2, userId);
        stmt.execute();
    }

    crow::json::wvalue res;
    res["message"] = "Password changed successfully";
    return {200, std::move(res)};
}

} // namespace account
