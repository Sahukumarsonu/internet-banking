-- Fictional demonstration data only. No real customer information.
-- All demo passwords below use the PBKDF2-SHA256 scheme implemented in
-- backend/auth.cpp with the DEFAULT dev pepper ("dev-only-pepper-change-me").
-- If you set PASSWORD_PEPPER to something else, these seeded logins will
-- stop working — either leave the pepper at its default for local/demo
-- use, or re-register these users through the API after changing it.

-- Admin account: admin@ibs.com / Admin@12345
INSERT INTO users (full_name, email, phone, password_hash, is_admin, is_active)
VALUES ('System Administrator', 'admin@ibs.com', '9999999999',
        'pbkdf2-sha256$210000$a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1$ba346be04df12e5fdb1afd11e3f608521e1ef0a75d1a176e3b373244d19cede8',
        1, 1);

-- Customer: John Doe — john.doe@example.com / Password@123
INSERT INTO users (full_name, email, phone, password_hash, is_admin, is_active)
VALUES ('John Doe', 'john.doe@example.com', '9876543210',
        'pbkdf2-sha256$210000$b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2$579784025bd7fba1cbc2eb37b86f407e5a62b78b238dd017d349dbdf3020d382',
        0, 1);

-- Customer: Priya Sharma — priya.sharma@example.com / Password@123
INSERT INTO users (full_name, email, phone, password_hash, is_admin, is_active)
VALUES ('Priya Sharma', 'priya.sharma@example.com', '9123456780',
        'pbkdf2-sha256$210000$c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3$088fe8903b532c41fb43c0ed1284553d683d69de188515bf3636bf73b079985b',
        0, 1);

-- Customer: Amit Kumar — amit.kumar@example.com / Password@123 (deactivated, for admin-panel demo)
INSERT INTO users (full_name, email, phone, password_hash, is_admin, is_active)
VALUES ('Amit Kumar', 'amit.kumar@example.com', '9012345678',
        'pbkdf2-sha256$210000$d4d4d4d4d4d4d4d4d4d4d4d4d4d4d4d4$e8dadc5be396b4b27244cfdc0ddb344d71900367f6eb6f5d5400f5dd9e0f50c5',
        0, 0);

-- Accounts (balances in paise). Admin has no account.
INSERT INTO accounts (user_id, account_number, balance_paise)
SELECT id, 'IBS100000000001', 5000000 FROM users WHERE email = 'john.doe@example.com';

INSERT INTO accounts (user_id, account_number, balance_paise)
SELECT id, 'IBS100000000002', 12500000 FROM users WHERE email = 'priya.sharma@example.com';

INSERT INTO accounts (user_id, account_number, balance_paise)
SELECT id, 'IBS100000000003', 0 FROM users WHERE email = 'amit.kumar@example.com';

-- Sample transaction history for John Doe.
INSERT INTO transactions (type, amount_paise, account_id, description, status, created_at)
SELECT 'deposit', 10000000, id, 'Initial cash deposit', 'completed', datetime('now', '-10 days')
FROM accounts WHERE account_number = 'IBS100000000001';

INSERT INTO transactions (type, amount_paise, account_id, description, status, created_at)
SELECT 'withdrawal', 500000, id, 'ATM withdrawal', 'completed', datetime('now', '-7 days')
FROM accounts WHERE account_number = 'IBS100000000001';

INSERT INTO transactions (type, amount_paise, account_id, counterparty_account_id, description, status, created_at)
SELECT 'transfer_out', 4500000,
       (SELECT id FROM accounts WHERE account_number = 'IBS100000000001'),
       (SELECT id FROM accounts WHERE account_number = 'IBS100000000002'),
       'Rent share', 'completed', datetime('now', '-3 days');

INSERT INTO transactions (type, amount_paise, account_id, counterparty_account_id, description, status, created_at)
SELECT 'transfer_in', 4500000,
       (SELECT id FROM accounts WHERE account_number = 'IBS100000000002'),
       (SELECT id FROM accounts WHERE account_number = 'IBS100000000001'),
       'Rent share', 'completed', datetime('now', '-3 days');
