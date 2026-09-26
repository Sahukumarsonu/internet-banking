-- Internet Banking System - SQLite Schema
-- Money is stored as INTEGER in paise (1 INR = 100 paise) to avoid floating point errors.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS users (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    full_name       TEXT NOT NULL,
    email           TEXT NOT NULL UNIQUE,
    phone           TEXT NOT NULL,
    password_hash   TEXT NOT NULL,
    is_active       INTEGER NOT NULL DEFAULT 1,     -- 1 = active, 0 = deactivated by admin
    is_admin        INTEGER NOT NULL DEFAULT 0,     -- 1 = admin user
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at      TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS accounts (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id         INTEGER NOT NULL UNIQUE,
    account_number  TEXT NOT NULL UNIQUE,
    balance_paise   INTEGER NOT NULL DEFAULT 0 CHECK (balance_paise >= 0),
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS transactions (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    type                TEXT NOT NULL CHECK (type IN ('deposit','withdrawal','transfer_out','transfer_in')),
    amount_paise        INTEGER NOT NULL CHECK (amount_paise > 0),
    account_id          INTEGER NOT NULL,           -- account this row belongs to (owner's view)
    counterparty_account_id INTEGER,                -- for transfers, the other account
    description         TEXT,
    status              TEXT NOT NULL DEFAULT 'completed' CHECK (status IN ('completed','failed','pending')),
    created_at          TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (account_id) REFERENCES accounts(id) ON DELETE CASCADE,
    FOREIGN KEY (counterparty_account_id) REFERENCES accounts(id)
);

CREATE TABLE IF NOT EXISTS sessions (
    token           TEXT PRIMARY KEY,
    user_id         INTEGER NOT NULL,
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    expires_at      TEXT NOT NULL,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS login_attempts (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    email           TEXT NOT NULL,
    attempted_at    TEXT NOT NULL DEFAULT (datetime('now')),
    success         INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_transactions_account_id ON transactions(account_id);
CREATE INDEX IF NOT EXISTS idx_transactions_created_at ON transactions(created_at);
CREATE INDEX IF NOT EXISTS idx_sessions_user_id ON sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_login_attempts_email_time ON login_attempts(email, attempted_at);
CREATE INDEX IF NOT EXISTS idx_accounts_account_number ON accounts(account_number);
