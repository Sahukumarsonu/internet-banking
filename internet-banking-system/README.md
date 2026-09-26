# Internet Banking System (C++ Full-Stack)

An educational, full-stack internet banking **simulation** built for an NTCC
college project. It uses fictional data only and does not connect to any
real bank, card network, or payment system.

- **Frontend:** HTML5, CSS3, vanilla JavaScript (responsive, no framework)
- **Backend:** C++17, [Crow](https://crowcpp.org/) REST framework, CMake
- **Database:** SQLite with prepared statements, money stored as integer paise
- **Deployment:** Frontend on GitHub Pages, backend on Render via Docker

> ⚠️ **Disclaimer:** This is a student project for demonstration purposes. It
> is not production-grade banking software and must never be connected to
> real financial systems, real customer data, or real money.

---

## 1. Project Structure

```
internet-banking-system/
├── frontend/                 # Static site — deployed to GitHub Pages
│   ├── index.html            # Landing page
│   ├── login.html            # Customer login
│   ├── register.html         # Customer registration
│   ├── dashboard.html        # Customer dashboard
│   ├── transfer.html         # Deposit / withdraw / transfer
│   ├── transactions.html     # Transaction history (search/filter/paginate)
│   ├── profile.html          # Profile + change password
│   ├── admin.html            # Admin login
│   ├── admin-dashboard.html  # Admin panel
│   ├── css/style.css
│   ├── js/
│   │   ├── api.js            # <-- API_BASE_URL configured here
│   │   ├── auth.js
│   │   ├── dashboard.js
│   │   ├── transfer.js
│   │   ├── transactions.js
│   │   ├── profile.js
│   │   └── admin.js
│   └── assets/
│
├── backend/                  # C++ REST API — deployed to Render
│   ├── main.cpp               # Route registration
│   ├── config.h                # Environment-variable configuration
│   ├── database.h / .cpp       # SQLite wrapper (Statement, Database)
│   ├── auth.h / .cpp            # Password hashing, sessions, rate limiting
│   ├── account.h / .cpp         # Profile / account endpoints
│   ├── transaction.h / .cpp     # Deposit / withdraw / transfer (atomic)
│   ├── CMakeLists.txt
│   └── Dockerfile
│
├── database/
│   ├── schema.sql             # Tables, constraints, indexes
│   └── seed.sql                # Fictional demo data (see credentials below)
│
├── .gitignore
├── docker-compose.yml         # Local dev convenience only
└── README.md
```

---

## 2. Local Development

### Prerequisites
- CMake ≥ 3.16, a C++17 compiler (g++/clang)
- `libsqlite3-dev`, `libssl-dev` (OpenSSL), `libboost-dev` + `libboost-system-dev`
- [Crow](https://github.com/CrowCpp/Crow) headers (header-only) — either install
  system-wide or drop them under `backend/third_party/crow/include`
- Python 3 (only for serving the frontend locally) or any static file server
- Docker + Docker Compose (optional, for the containerized workflow)

### Option A — Docker Compose (recommended, matches production)
```bash
docker compose up --build
# Backend:  http://localhost:8080
# Frontend: http://localhost:8081
```
The frontend's `js/api.js` auto-detects `localhost` and points at
`http://localhost:8080`, so no manual edits are needed for local dev.

### Option B — Build natively
```bash
# 1. Install dependencies (Debian/Ubuntu example)
sudo apt-get update && sudo apt-get install -y \
  build-essential cmake libsqlite3-dev libssl-dev libboost-dev libboost-system-dev git

# 2. Fetch Crow headers
git clone --depth 1 --branch v1.2.0 https://github.com/CrowCpp/Crow.git /tmp/crow
sudo cp -r /tmp/crow/include/* /usr/local/include/

# 3. Build
cd backend
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

# 4. Run (schema.sql is applied automatically on first boot)
export BANK_DB_PATH=./bank.db
export PORT=8080
export ALLOWED_ORIGINS=*
./build/bank_server
```

### Serve the frontend locally
```bash
cd frontend
python3 -m http.server 8081
# open http://localhost:8081
```

---

## 3. Database Setup

The backend automatically applies `database/schema.sql` on startup
(`CREATE TABLE IF NOT EXISTS`, so it's safe to run every boot). To load the
fictional demo data:

```bash
sqlite3 backend/bank.db < database/seed.sql
```

### Demo credentials (fictional data, PASSWORD_PEPPER left at its default)
| Role     | Email                         | Password       |
|----------|-------------------------------|----------------|
| Admin    | admin@ibs.com                 | Admin@12345    |
| Customer | john.doe@example.com          | Password@123   |
| Customer | priya.sharma@example.com      | Password@123   |
| Customer (deactivated) | amit.kumar@example.com | Password@123 |

If you change `PASSWORD_PEPPER` from its default, these seeded password
hashes will stop verifying — either keep the default pepper for demo use,
or re-register these users through `/api/auth/register` after changing it.

Money is stored as **integer paise** (`balance_paise`) to avoid
floating-point rounding errors; the API converts to/from rupees at the edge.

---

## 4. API Documentation

Base path: `/api`. All authenticated routes require
`Authorization: Bearer <token>` (token returned by login).

| Method | Endpoint | Auth | Description |
|--------|----------|------|-------------|
| POST | `/auth/register` | — | Create a customer + account |
| POST | `/auth/login` | — | Log in, returns session token |
| POST | `/auth/logout` | Bearer | Invalidate current session |
| GET  | `/auth/me` | Bearer | Current user info |
| GET  | `/account` | Bearer | Account number, balance, created date |
| GET  | `/account/balance` | Bearer | Balance only |
| GET  | `/profile` | Bearer | Profile details |
| PUT  | `/profile` | Bearer | Update full name / phone |
| PUT  | `/profile/password` | Bearer | Change password |
| POST | `/transactions/deposit` | Bearer | `{ amount, description? }` |
| POST | `/transactions/withdraw` | Bearer | `{ amount, description? }` |
| POST | `/transactions/transfer` | Bearer | `{ receiverAccountNumber, amount, description? }` |
| GET  | `/transactions` | Bearer | Query: `type, from, to, page, pageSize` |
| GET  | `/transactions/{id}` | Bearer | Single transaction (must belong to caller) |
| POST | `/admin/login` | — | Admin login (checks `is_admin`) |
| GET  | `/admin/dashboard` | Bearer (admin) | Aggregate stats |
| GET  | `/admin/customers` | Bearer (admin) | Query: `search` |
| GET  | `/admin/transactions` | Bearer (admin) | Query: `limit` |
| PUT  | `/admin/customers/{id}/status` | Bearer (admin) | `{ isActive }` |
| GET  | `/health` | — | Health check for uptime monitors |

### Testing with curl
```bash
# Register
curl -X POST http://localhost:8080/api/auth/register \
  -H "Content-Type: application/json" \
  -d '{"fullName":"Test User","email":"test@example.com","phone":"9876543210","password":"Password@123","confirmPassword":"Password@123"}'

# Login
curl -X POST http://localhost:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email":"test@example.com","password":"Password@123"}'
# -> save the "token" from the response

# Deposit
curl -X POST http://localhost:8080/api/transactions/deposit \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <TOKEN>" \
  -d '{"amount": 500, "description": "Test deposit"}'

# Transaction history
curl http://localhost:8080/api/transactions \
  -H "Authorization: Bearer <TOKEN>"
```
Or import the same requests into Postman using the table above.

---

## 5. Security Notes

- Passwords are hashed with **PBKDF2-HMAC-SHA256** (210,000 iterations) plus a
  server-side pepper, via OpenSSL. *(The original brief suggested Argon2id or
  bcrypt; PBKDF2 was used instead so the project builds cleanly on Render's
  container without vendoring a native Argon2/bcrypt library. It's still a
  reputable, industry-standard KDF for this purpose. To switch, only
  `auth.cpp`'s `hashPassword`/`verifyPassword` need to change.)*
- All SQL uses prepared statements (no string-concatenated queries).
- The backend recalculates and re-checks balances itself — the frontend
  never sends a balance, and withdrawals/transfers are guarded by a
  `WHERE balance_paise >= ?` clause so concurrent requests can't overdraw.
- Transfers are wrapped in a single SQLite transaction (`BEGIN`/`COMMIT`);
  any failure triggers a `ROLLBACK`, so both balances always move together
  or neither does.
- Login attempts are rate-limited per email (`MAX_LOGIN_ATTEMPTS` failures
  within `LOGIN_LOCKOUT_WINDOW_MINUTES`).
- Sessions are random 256-bit tokens stored server-side with an expiry
  (`SESSION_LIFETIME_HOURS`), not JWTs — this keeps logout/revocation simple.
- CORS is restricted via `ALLOWED_ORIGINS`; set it to your exact GitHub
  Pages origin in production, not `*`.
- No secrets are committed to source control — everything sensitive is an
  environment variable (see `.gitignore` and `config.h`).

---

## 6. Frontend Configuration

All API calls go through `frontend/js/api.js`. **This is the only file you
need to edit when deploying:**

```js
const API_BASE_URL = (function () {
  if (window.location.hostname === "localhost" || window.location.hostname === "127.0.0.1") {
    return "http://localhost:8080";
  }
  return "https://YOUR-RENDER-SERVICE.onrender.com"; // <-- change this after deploying the backend
})();
```

---

## 7. GitHub Pages Deployment (Frontend)

GitHub Pages **only serves static files — it cannot run the C++ backend.**
You must deploy the backend separately (see Section 8) and point the
frontend at it.

1. Push this repository to GitHub.
2. In your repo: **Settings → Pages → Source** → select the branch and set
   the folder to `/frontend` (or use a GitHub Action to publish that folder
   to the `gh-pages` branch — either works).
3. Wait for the Pages build to finish; your site will be at
   `https://<username>.github.io/<repo-name>/`.
4. Edit `frontend/js/api.js` and set `API_BASE_URL` to your deployed Render
   URL (Section 8), then commit and push — GitHub Pages redeploys
   automatically.
5. To update the site after future changes: just push to the branch/folder
   Pages is configured to serve; no manual rebuild step is needed since it's
   plain static HTML/CSS/JS.

All internal links and asset references use **relative paths**
(`css/style.css`, `js/api.js`, `login.html`, …), so the site works correctly
whether it's served from a domain root or from a GitHub Pages subpath like
`/internet-banking-system/`.

---

## 8. Render Deployment (C++ Backend)

1. Push this repository to GitHub (same repo as the frontend is fine).
2. In Render: **New → Web Service** → connect your GitHub repo.
3. **Environment:** Docker. **Dockerfile path:** `backend/Dockerfile`.
   **Docker build context:** repository root (so the Dockerfile can `COPY`
   both `backend/` and `database/`).
4. **Environment variables** (Render dashboard → Environment):
   | Key | Example value |
   |---|---|
   | `ALLOWED_ORIGINS` | `https://<username>.github.io` |
   | `PASSWORD_PEPPER` | a long random string you generate |
   | `SESSION_LIFETIME_HOURS` | `12` |
   | `BANK_DB_PATH` | `/data/bank.db` |

   Render automatically injects `PORT`; `config.h` reads it and the server
   binds to `0.0.0.0:$PORT` as required.
5. **Persistent storage:** SQLite writes to a file, and Render's filesystem
   is **ephemeral** — it resets on every deploy/restart. To keep data:
   - Go to your service → **Disks** → add a disk, e.g. mounted at `/data`.
   - Keep `BANK_DB_PATH=/data/bank.db` so the database lives on that disk.
   - Without a disk, the app still works, but all data (including the
     seeded demo users) is wiped on every redeploy — acceptable for a quick
     demo, not for anything you want to persist.
   - If your grading environment doesn't support persistent disks at all,
     an alternative is swapping SQLite for Render's managed PostgreSQL —
     that would require changing `database.cpp`'s SQL dialect (mainly
     `AUTOINCREMENT` → `SERIAL`/`IDENTITY` and the `datetime('now')` calls)
     and linking `libpq` instead of `libsqlite3` in `CMakeLists.txt`.
6. Deploy. Render will build the Docker image (this takes a few minutes the
   first time since it compiles Crow + the app from source).
7. **View logs:** Render dashboard → your service → **Logs** tab (live
   tail of stdout/stderr, including the "listening on 0.0.0.0:PORT" line).
8. **Test the deployed API:**
   ```bash
   curl https://YOUR-RENDER-SERVICE.onrender.com/api/health
   ```
9. Update `frontend/js/api.js` with this same URL (Section 6) and push.

---

## 9. Final Checklist — verify the site works from another device

- [ ] `GET https://YOUR-RENDER-SERVICE.onrender.com/api/health` returns `{"status":"ok",...}`
- [ ] `frontend/js/api.js` has `API_BASE_URL` set to that Render URL
- [ ] GitHub Pages is enabled and serving `frontend/`
- [ ] From a phone or a different network, open the GitHub Pages URL
- [ ] Register a new account → land back on login with a success toast
- [ ] Log in → dashboard shows ₹0.00 balance and your generated account number
- [ ] Deposit money → balance and recent transactions update
- [ ] Withdraw more than your balance → see a friendly "insufficient balance" error, no crash
- [ ] Open a second account (or use a seeded demo account) and transfer money between the two → both balances update correctly
- [ ] Transaction history page: search, filter by type, filter by date, and paginate all work
- [ ] Log out, then try visiting `dashboard.html` directly → redirected to login
- [ ] Admin login (`admin.html`) with `admin@ibs.com` → dashboard shows customer/account/transaction totals
- [ ] Deactivate a customer from the admin panel → that customer can no longer log in

---

## 10. Testing Notes

Manually verified request/response scenarios (see Section 4 for curl
examples) that map to the required test cases:

- Successful registration / duplicate registration (`409 Conflict`)
- Successful login / invalid password (`401`) / too many attempts (`429`)
- Successful deposit / successful withdrawal
- Withdrawal with insufficient balance (`400`, balance unchanged — the
  `WHERE balance_paise >= ?` guard prevents the row from updating at all)
- Successful transfer / transfer to a non-existent account (`400`) /
  transfer to your own account (`400`)
- Accessing another user's transaction by ID (`404` — ownership is checked
  via `account_id`, not trusted from the client)
- Transaction history pagination and filters
- Non-admin hitting an `/admin/*` route (`403`)
- Invalid input (bad email format, weak password, non-numeric amount, etc.)
- A deliberately failed multi-statement write inside a transfer is rolled
  back atomically (verified by forcing a constraint violation mid-transfer
  in development and confirming both balances stayed unchanged)

---

## 11. Future Improvements

- Move from SQLite to PostgreSQL for true concurrent-write scalability
- Add refresh tokens / shorter-lived access tokens instead of one long-lived
  session token
- Add email verification on registration
- Add downloadable PDF/CSV statements
- Add scheduled/recurring transfers
- Add two-factor authentication for login
- Replace PBKDF2 with Argon2id (see Security Notes) if a suitable
  cross-platform build of libargon2 is available in your deployment target

---

## 12. Screenshots

_Add screenshots of the home page, dashboard, transfer form, transaction
history, and admin panel here once you've run the app locally or deployed
it — e.g. `![Dashboard](docs/screenshots/dashboard.png)`._
