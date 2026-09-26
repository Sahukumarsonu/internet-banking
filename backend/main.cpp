// Internet Banking System — C++ backend
// Educational banking SIMULATION using fictional data. Does not connect
// to any real bank or payment network.

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "config.h"
#include "database.h"
#include "auth.h"
#include "account.h"
#include "transaction.h"
#include <iostream>
#include <optional>
#include <cstdlib>
#include <vector>
#include <mutex>
#include <string>

namespace {

// Extracts the bearer token from the Authorization header.
std::string extractToken(const crow::request& req) {
    std::string header = req.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    if (header.rfind(prefix, 0) == 0) {
        return header.substr(prefix.size());
    }
    return "";
}

crow::json::wvalue jsonError(const std::string& message) {
    crow::json::wvalue err;
    err["error"] = message;
    return err;
}

// Resolves the current user or returns std::nullopt and writes a 401.
std::optional<auth::SessionUser> requireAuth(const crow::request& req, crow::response& res) {
    std::string token = extractToken(req);
    auto user = auth::getUserFromToken(token);
    if (!user) {
        res.code = 401;
        res.write(jsonError("Unauthorized. Please log in.").dump());
        return std::nullopt;
    }
    if (!user->isActive) {
        res.code = 403;
        res.write(jsonError("Your account has been deactivated. Contact support.").dump());
        return std::nullopt;
    }
    return user;
}

std::optional<auth::SessionUser> requireAdmin(const crow::request& req, crow::response& res) {
    auto user = requireAuth(req, res);
    if (!user) return std::nullopt;
    if (!user->isAdmin) {
        res.code = 403;
        res.write(jsonError("Admin access required").dump());
        return std::nullopt;
    }
    return user;
}

void sendJson(crow::response& res, int status, crow::json::wvalue&& body) {
    res.code = status;
    res.set_header("Content-Type", "application/json");
    res.write(body.dump());
    res.end();
}

} // namespace

int main() {
    // --- Database setup -----------------------------------------------
    auto& db = Database::instance();
    if (!db.open(config::dbPath())) {
        std::cerr << "FATAL: could not open database at " << config::dbPath() << std::endl;
        return 1;
    }
    // Auto-apply schema on boot so a fresh deploy "just works". Statements
    // use CREATE TABLE IF NOT EXISTS, so this is safe to run every start.
    if (!db.runSqlFile("../database/schema.sql") && !db.runSqlFile("./database/schema.sql")) {
        std::cerr << "WARNING: could not apply schema.sql automatically. "
                     "Make sure the schema has been applied to the database." << std::endl;
    }

    crow::App<crow::CORSHandler> app;

    // --- CORS ------------------------------------------------------------
    auto& cors = app.get_middleware<crow::CORSHandler>();
    {
        std::string origins = config::allowedOrigins();
        cors.global()
            .headers("Content-Type", "Authorization")
            .methods("GET"_method, "POST"_method, "PUT"_method, "DELETE"_method, "OPTIONS"_method)
            .origin(origins);
    }

    // ======================================================================
    // AUTH ROUTES
    // ======================================================================

    CROW_ROUTE(app, "/api/auth/register").methods("POST"_method)
    ([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, R"({"error":"Invalid JSON"})");

        if (!body.has("fullName") || !body.has("email") || !body.has("phone") ||
            !body.has("password") || !body.has("confirmPassword")) {
            crow::json::wvalue err; err["error"] = "All fields are required";
            return crow::response(400, err.dump());
        }

        std::string fullName = body["fullName"].s();
        std::string email = body["email"].s();
        std::string phone = body["phone"].s();
        std::string password = body["password"].s();
        std::string confirmPassword = body["confirmPassword"].s();

        if (fullName.empty() || fullName.size() > 100) {
            crow::json::wvalue err; err["error"] = "Invalid full name"; return crow::response(400, err.dump());
        }
        if (!auth::isValidEmail(email)) {
            crow::json::wvalue err; err["error"] = "Invalid email address"; return crow::response(400, err.dump());
        }
        if (!auth::isValidPhone(phone)) {
            crow::json::wvalue err; err["error"] = "Invalid phone number"; return crow::response(400, err.dump());
        }
        if (password != confirmPassword) {
            crow::json::wvalue err; err["error"] = "Passwords do not match"; return crow::response(400, err.dump());
        }
        if (!auth::isStrongEnoughPassword(password)) {
            crow::json::wvalue err; err["error"] = "Password must be at least 8 characters"; return crow::response(400, err.dump());
        }

        auto& db = Database::instance();
        std::lock_guard<std::mutex> lock(db.writeMutex());

        // Duplicate email check
        {
            Statement check(db.handle(), "SELECT id FROM users WHERE email = ?");
            check.bindText(1, email);
            if (check.step()) {
                crow::json::wvalue err; err["error"] = "An account with this email already exists";
                return crow::response(409, err.dump());
            }
        }

        std::string passwordHash = auth::hashPassword(password);

        if (!db.begin()) {
            crow::json::wvalue err; err["error"] = "Registration failed"; return crow::response(500, err.dump());
        }

        Statement insUser(db.handle(),
            "INSERT INTO users (full_name, email, phone, password_hash) VALUES (?, ?, ?, ?)");
        insUser.bindText(1, fullName);
        insUser.bindText(2, email);
        insUser.bindText(3, phone);
        insUser.bindText(4, passwordHash);
        if (!insUser.execute()) {
            db.rollback();
            crow::json::wvalue err; err["error"] = "Registration failed"; return crow::response(500, err.dump());
        }
        long long userId = sqlite3_last_insert_rowid(db.handle());

        std::string accountNumber = auth::generateAccountNumber();
        Statement insAcc(db.handle(),
            "INSERT INTO accounts (user_id, account_number, balance_paise) VALUES (?, ?, 0)");
        insAcc.bindInt(1, userId);
        insAcc.bindText(2, accountNumber);
        if (!insAcc.execute()) {
            db.rollback();
            crow::json::wvalue err; err["error"] = "Registration failed"; return crow::response(500, err.dump());
        }

        if (!db.commit()) {
            db.rollback();
            crow::json::wvalue err; err["error"] = "Registration failed"; return crow::response(500, err.dump());
        }

        crow::json::wvalue result;
        result["message"] = "Registration successful";
        result["accountNumber"] = accountNumber;
        return crow::response(201, result.dump());
    });

    CROW_ROUTE(app, "/api/auth/login").methods("POST"_method)
    ([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("email") || !body.has("password")) {
            crow::json::wvalue err; err["error"] = "email and password are required";
            return crow::response(400, err.dump());
        }
        std::string email = body["email"].s();
        std::string password = body["password"].s();

        if (!auth::isLoginAllowed(email)) {
            crow::json::wvalue err;
            err["error"] = "Too many failed login attempts. Please try again later.";
            return crow::response(429, err.dump());
        }

        auto& db = Database::instance();
        Statement stmt(db.handle(),
            "SELECT id, password_hash, full_name, is_admin, is_active FROM users WHERE email = ?");
        stmt.bindText(1, email);

        if (!stmt.step()) {
            auth::recordLoginAttempt(email, false);
            crow::json::wvalue err; err["error"] = "Invalid email or password";
            return crow::response(401, err.dump());
        }

        long long userId = stmt.columnInt(0);
        std::string storedHash = stmt.columnText(1);
        std::string fullName = stmt.columnText(2);
        bool isAdmin = stmt.columnInt(3) != 0;
        bool isActive = stmt.columnInt(4) != 0;

        if (!auth::verifyPassword(password, storedHash)) {
            auth::recordLoginAttempt(email, false);
            crow::json::wvalue err; err["error"] = "Invalid email or password";
            return crow::response(401, err.dump());
        }

        if (!isActive) {
            auth::recordLoginAttempt(email, false);
            crow::json::wvalue err; err["error"] = "Your account has been deactivated. Contact support.";
            return crow::response(403, err.dump());
        }

        auth::recordLoginAttempt(email, true);
        std::string token = auth::createSession(userId);

        crow::json::wvalue result;
        result["token"] = token;
        result["user"]["id"] = userId;
        result["user"]["fullName"] = fullName;
        result["user"]["email"] = email;
        result["user"]["isAdmin"] = isAdmin;
        return crow::response(200, result.dump());
    });

    CROW_ROUTE(app, "/api/auth/logout").methods("POST"_method)
    ([](const crow::request& req) {
        std::string token = extractToken(req);
        auth::destroySession(token);
        crow::json::wvalue result; result["message"] = "Logged out";
        return crow::response(200, result.dump());
    });

    CROW_ROUTE(app, "/api/auth/me").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        auto user = requireAuth(req, res);
        if (!user) { res.end(); return; }
        crow::json::wvalue result;
        result["id"] = user->userId;
        result["fullName"] = user->fullName;
        result["email"] = user->email;
        result["isAdmin"] = user->isAdmin;
        sendJson(res, 200, std::move(result));
    });

    // ======================================================================
    // ACCOUNT / PROFILE ROUTES
    // ======================================================================

    CROW_ROUTE(app, "/api/account").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        auto user = requireAuth(req, res);
        if (!user) { res.end(); return; }
        auto r = account::getAccount(user->userId);
        sendJson(res, r.status, std::move(r.body));
    });

    CROW_ROUTE(app, "/api/account/balance").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        auto user = requireAuth(req, res);
        if (!user) { res.end(); return; }
        auto r = account::getBalance(user->userId);
        sendJson(res, r.status, std::move(r.body));
    });

    CROW_ROUTE(app, "/api/profile").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        auto user = requireAuth(req, res);
        if (!user) { res.end(); return; }
        auto r = account::getProfile(user->userId);
        sendJson(res, r.status, std::move(r.body));
    });

    CROW_ROUTE(app, "/api/profile").methods("PUT"_method)
    ([](const crow::request& req, crow::response& res) {
        auto user = requireAuth(req, res);
        if (!user) { res.end(); return; }
        auto body = crow::json::load(req.body);
        if (!body) { sendJson(res, 400, jsonError("Invalid JSON")); return; }
        auto r = account::updateProfile(user->userId, body);
        sendJson(res, r.status, std::move(r.body));
    });

    CROW_ROUTE(app, "/api/profile/password").methods("PUT"_method)
    ([](const crow::request& req, crow::response& res) {
        auto user = requireAuth(req, res);
        if (!user) { res.end(); return; }
        auto body = crow::json::load(req.body);
        if (!body) { sendJson(res, 400, jsonError("Invalid JSON")); return; }
        auto r = account::changePassword(user->userId, body);
        sendJson(res, r.status, std::move(r.body));
    });

    // ======================================================================
    // BANKING ROUTES
    // ======================================================================

    CROW_ROUTE(app, "/api/transactions/deposit").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        auto user = requireAuth(req, res);
        if (!user) { res.end(); return; }
        auto body = crow::json::load(req.body);
        if (!body) { sendJson(res, 400, jsonError("Invalid JSON")); return; }
        auto r = transaction::deposit(user->userId, body);
        sendJson(res, r.status, std::move(r.body));
    });

    CROW_ROUTE(app, "/api/transactions/withdraw").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        auto user = requireAuth(req, res);
        if (!user) { res.end(); return; }
        auto body = crow::json::load(req.body);
        if (!body) { sendJson(res, 400, jsonError("Invalid JSON")); return; }
        auto r = transaction::withdraw(user->userId, body);
        sendJson(res, r.status, std::move(r.body));
    });

    CROW_ROUTE(app, "/api/transactions/transfer").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        auto user = requireAuth(req, res);
        if (!user) { res.end(); return; }
        auto body = crow::json::load(req.body);
        if (!body) { sendJson(res, 400, jsonError("Invalid JSON")); return; }
        auto r = transaction::transfer(user->userId, body);
        sendJson(res, r.status, std::move(r.body));
    });

    CROW_ROUTE(app, "/api/transactions").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        auto user = requireAuth(req, res);
        if (!user) { res.end(); return; }
        auto r = transaction::listTransactions(user->userId, req.url_params);
        sendJson(res, r.status, std::move(r.body));
    });

    CROW_ROUTE(app, "/api/transactions/<int>").methods("GET"_method)
    ([](const crow::request& req, crow::response& res, int id) {
        auto user = requireAuth(req, res);
        if (!user) { res.end(); return; }
        auto r = transaction::getTransactionById(user->userId, id);
        sendJson(res, r.status, std::move(r.body));
    });

    // ======================================================================
    // ADMIN ROUTES
    // ======================================================================

    CROW_ROUTE(app, "/api/admin/login").methods("POST"_method)
    ([](const crow::request& req) {
        // Admin login reuses the same credential store but requires is_admin = 1.
        auto body = crow::json::load(req.body);
        if (!body || !body.has("email") || !body.has("password")) {
            crow::json::wvalue err; err["error"] = "email and password are required";
            return crow::response(400, err.dump());
        }
        std::string email = body["email"].s();
        std::string password = body["password"].s();

        if (!auth::isLoginAllowed(email)) {
            crow::json::wvalue err;
            err["error"] = "Too many failed login attempts. Please try again later.";
            return crow::response(429, err.dump());
        }

        auto& db = Database::instance();
        Statement stmt(db.handle(),
            "SELECT id, password_hash, full_name, is_admin, is_active FROM users WHERE email = ?");
        stmt.bindText(1, email);

        if (!stmt.step() || !auth::verifyPassword(password, stmt.columnText(1))) {
            auth::recordLoginAttempt(email, false);
            crow::json::wvalue err; err["error"] = "Invalid email or password";
            return crow::response(401, err.dump());
        }

        bool isAdmin = stmt.columnInt(3) != 0;
        if (!isAdmin) {
            auth::recordLoginAttempt(email, false);
            crow::json::wvalue err; err["error"] = "This account does not have admin access";
            return crow::response(403, err.dump());
        }

        long long userId = stmt.columnInt(0);
        auth::recordLoginAttempt(email, true);
        std::string token = auth::createSession(userId);

        crow::json::wvalue result;
        result["token"] = token;
        result["user"]["id"] = userId;
        result["user"]["fullName"] = stmt.columnText(2);
        result["user"]["email"] = email;
        result["user"]["isAdmin"] = true;
        return crow::response(200, result.dump());
    });

    CROW_ROUTE(app, "/api/admin/dashboard").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        auto admin = requireAdmin(req, res);
        if (!admin) { res.end(); return; }
        auto& db = Database::instance();

        crow::json::wvalue result;
        {
            Statement s(db.handle(), "SELECT COUNT(*) FROM users WHERE is_admin = 0");
            s.step(); result["totalCustomers"] = s.columnInt(0);
        }
        {
            Statement s(db.handle(), "SELECT COUNT(*) FROM accounts");
            s.step(); result["totalAccounts"] = s.columnInt(0);
        }
        {
            Statement s(db.handle(), "SELECT COUNT(*) FROM transactions");
            s.step(); result["totalTransactions"] = s.columnInt(0);
        }
        {
            Statement s(db.handle(), "SELECT COALESCE(SUM(balance_paise), 0) FROM accounts");
            s.step(); result["totalDepositsHeld"] = s.columnInt(0) / 100.0;
        }
        sendJson(res, 200, std::move(result));
    });

    CROW_ROUTE(app, "/api/admin/customers").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        auto admin = requireAdmin(req, res);
        if (!admin) { res.end(); return; }
        auto& db = Database::instance();

        std::string search = req.url_params.get("search") ? std::string(req.url_params.get("search")) : "";
        std::string sql =
            "SELECT u.id, u.full_name, u.email, u.phone, u.is_active, u.created_at, "
            "a.account_number, a.balance_paise "
            "FROM users u JOIN accounts a ON a.user_id = u.id "
            "WHERE u.is_admin = 0";
        if (!search.empty()) {
            sql += " AND (u.full_name LIKE ? OR u.email LIKE ? OR a.account_number LIKE ?)";
        }
        sql += " ORDER BY u.created_at DESC";

        Statement stmt(db.handle(), sql);
        if (!search.empty()) {
            std::string pattern = "%" + search + "%";
            stmt.bindText(1, pattern);
            stmt.bindText(2, pattern);
            stmt.bindText(3, pattern);
        }

        std::vector<crow::json::wvalue> customers;
        while (stmt.step()) {
            crow::json::wvalue c;
            c["id"] = stmt.columnInt(0);
            c["fullName"] = stmt.columnText(1);
            c["email"] = stmt.columnText(2);
            c["phone"] = stmt.columnText(3);
            c["isActive"] = stmt.columnInt(4) != 0;
            c["createdAt"] = stmt.columnText(5);
            c["accountNumber"] = stmt.columnText(6);
            c["balance"] = stmt.columnInt(7) / 100.0;
            customers.push_back(std::move(c));
        }
        crow::json::wvalue result;
        result["customers"] = std::move(customers);
        sendJson(res, 200, std::move(result));
    });

    CROW_ROUTE(app, "/api/admin/transactions").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        auto admin = requireAdmin(req, res);
        if (!admin) { res.end(); return; }
        auto& db = Database::instance();

        int limit = req.url_params.get("limit") ? std::atoi(req.url_params.get("limit")) : 50;
        if (limit <= 0 || limit > 200) limit = 50;

        Statement stmt(db.handle(),
            "SELECT t.id, t.type, t.amount_paise, t.description, t.status, t.created_at, "
            "a.account_number, u.full_name "
            "FROM transactions t "
            "JOIN accounts a ON a.id = t.account_id "
            "JOIN users u ON u.id = a.user_id "
            "ORDER BY t.created_at DESC LIMIT ?");
        stmt.bindInt(1, limit);

        std::vector<crow::json::wvalue> items;
        while (stmt.step()) {
            crow::json::wvalue row;
            row["id"] = stmt.columnInt(0);
            row["type"] = stmt.columnText(1);
            row["amount"] = stmt.columnInt(2) / 100.0;
            row["description"] = stmt.columnText(3);
            row["status"] = stmt.columnText(4);
            row["createdAt"] = stmt.columnText(5);
            row["accountNumber"] = stmt.columnText(6);
            row["customerName"] = stmt.columnText(7);
            items.push_back(std::move(row));
        }
        crow::json::wvalue result;
        result["transactions"] = std::move(items);
        sendJson(res, 200, std::move(result));
    });

    CROW_ROUTE(app, "/api/admin/customers/<int>/status").methods("PUT"_method)
    ([](const crow::request& req, crow::response& res, int customerId) {
        auto admin = requireAdmin(req, res);
        if (!admin) { res.end(); return; }
        auto body = crow::json::load(req.body);
        if (!body || !body.has("isActive")) {
            sendJson(res, 400, jsonError("isActive (boolean) is required"));
            return;
        }
        bool isActive = body["isActive"].b();

        auto& db = Database::instance();
        std::lock_guard<std::mutex> lock(db.writeMutex());
        Statement stmt(db.handle(),
            "UPDATE users SET is_active = ?, updated_at = datetime('now') WHERE id = ? AND is_admin = 0");
        stmt.bindInt(1, isActive ? 1 : 0);
        stmt.bindInt(2, customerId);
        stmt.execute();

        if (sqlite3_changes(db.handle()) == 0) {
            sendJson(res, 404, jsonError("Customer not found"));
            return;
        }

        crow::json::wvalue result;
        result["message"] = isActive ? "Customer activated" : "Customer deactivated";
        sendJson(res, 200, std::move(result));
    });

    // Simple health check for Render / uptime monitors.
    CROW_ROUTE(app, "/api/health").methods("GET"_method)
    ([]() {
        crow::json::wvalue result;
        result["status"] = "ok";
        result["service"] = "internet-banking-system";
        return crow::response(200, result.dump());
    });

    int port = config::port();
    std::cout << "Internet Banking System backend listening on 0.0.0.0:" << port << std::endl;
    app.bindaddr("0.0.0.0").port(port).multithreaded().run();

    return 0;
}
