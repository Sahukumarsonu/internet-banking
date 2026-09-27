# Internet Banking System (C++ Full-Stack)

An educational, full-stack internet banking **simulation** for an NTCC
college project, using fictional data only.

- **Frontend:** `https://sahukumarsonu.github.io/internet-banking/`
- **Backend:** `https://internet-banking-ek6f.onrender.com`

Both URLs are hardcoded into this project (see `frontend/js/api.js` and
`backend/config.h`) — deploying it as-is, to the same GitHub Pages repo
and the same Render service, should work with no manual configuration.

> ⚠️ Disclaimer: this is a student project for demonstration purposes. It
> is not production-grade banking software and must never be connected to
> real financial systems, real customer data, or real money.

---

## What's hardcoded, and why

This rewrite followed a long debugging session that surfaced several real,
separate issues in a row. Each fix below is now baked into the code from
the start, not something you need to configure:

1. **`frontend/js/api.js`** — `PRODUCTION_API_URL` is set directly to
   `https://internet-banking-ek6f.onrender.com`. (Still auto-detects
   `localhost` for local dev and uses `http://localhost:8080` there.)
2. **`backend/config.h`** — `ALLOWED_ORIGINS` defaults to
   `https://sahukumarsonu.github.io` if the env var isn't set on Render,
   instead of a permissive `*` or an unset value. You can still override it
   via Render's Environment tab if you ever move the frontend.
3. **`backend/main.cpp`** — uses a small hand-written CORS middleware
   (`SimpleCors`) instead of Crow's built-in `CORSHandler`, plus an
   explicit catch-all `OPTIONS` route under `/api/<path>`. In testing,
   Crow's built-in CORS middleware silently failed to answer preflight
   requests that had no other matching route, and a plain `204 No Content`
   preflight reply was intermittently rejected by Render's edge (Cloudflare)
   as malformed — this version replies `200` with an explicit empty body,
   which is more universally tolerated.
4. **`frontend/js/api.js` + `auth.js` + `admin.js`** — the login, register,
   and admin-login calls send `Content-Type: text/plain` instead of
   `application/json` (via a `skipPreflight` flag). Since those calls don't
   need an `Authorization` header, this makes them "simple" CORS requests
   that skip the preflight `OPTIONS` round-trip entirely, sidestepping the
   Render edge flakiness described above for the most important calls.
5. **Automatic retry on network failure** — `Api.getWithRetry` /
   `postWithRetry` / `putWithRetry` retry exactly once, after a 1.5s delay,
   *only* when the browser's `fetch()` fails at the network level (not on
   a real HTTP error response). This covers cases where free-tier hosting
   drops a request or response in transit even though the server actually
   processed it. **Only used for safe, idempotent calls** — every read
   (GET) and login/register. Money-moving writes (`deposit`, `withdraw`,
   `transfer`) and the password-change endpoint deliberately do **not**
   auto-retry, since silently resubmitting one of those after an unclear
   network failure could duplicate a real transaction. Those show a
   message asking you to check your balance/history before resubmitting.

None of this eliminates Render's free-tier cold-start/edge behavior — that's
inherent to the hosting tier, not a code bug — but it makes the app behave
sensibly around it instead of leaving you guessing.

---

## Project Structure

```
internet-banking-system/
├── frontend/                 # Static site — GitHub Pages
│   ├── index.html, login.html, register.html, dashboard.html,
│   │   transfer.html, transactions.html, profile.html,
│   │   admin.html, admin-dashboard.html
│   ├── css/style.css
│   └── js/
│       ├── api.js            # API_BASE_URL + fetch wrapper + retry logic
│       ├── auth.js, dashboard.js, transfer.js,
│       │   transactions.js, profile.js, admin.js
│
├── backend/                  # C++ REST API — Render (Docker)
│   ├── main.cpp               # Routes + SimpleCors middleware
│   ├── config.h                 # Env-var configuration (hardcoded defaults)
│   ├── database.h / .cpp         # SQLite wrapper
│   ├── auth.h / .cpp              # Password hashing, sessions, rate limiting
│   ├── account.h / .cpp            # Profile / account endpoints
│   ├── transaction.h / .cpp         # Deposit / withdraw / transfer (atomic)
│   ├── CMakeLists.txt
│   └── Dockerfile              # Includes libasio-dev — required for this
│                                  Crow version to build cleanly on Render
├── database/
│   ├── schema.sql
│   └── seed.sql                # Fictional demo data (see credentials below)
│
├── .gitignore
├── docker-compose.yml
└── README.md
```

---

## Local Development

```bash
docker compose up --build
# Backend:  http://localhost:8080
# Frontend: http://localhost:8081
```
`js/api.js` auto-detects `localhost` and points there — no edits needed.

To build natively instead, see the "Local Development" section pattern:
install `build-essential cmake libsqlite3-dev libssl-dev libboost-dev
libboost-system-dev libasio-dev`, clone Crow's headers into
`/usr/local/include`, then `cmake -B build && cmake --build build` from
`backend/`.

---

## Demo credentials (fictional data)

| Role     | Email                         | Password       |
|----------|-------------------------------|----------------|
| Admin    | admin@ibs.com                 | Admin@12345    |
| Customer | john.doe@example.com          | Password@123   |
| Customer | priya.sharma@example.com      | Password@123   |
| Customer (deactivated) | amit.kumar@example.com | Password@123 |

---

## Deploying this exact project

### Frontend (GitHub Pages)
1. Push this repo to `sahukumarsonu/internet-banking` on `main`.
2. Settings → Pages → serve the `frontend/` folder (or via a workflow to
   `gh-pages`, whichever this repo is already using).
3. No further edits needed — `api.js` already points at the right backend.

### Backend (Render)
1. New Web Service → Docker → Dockerfile path `backend/Dockerfile`,
   build context = repo root.
2. Environment variables (optional — sensible defaults are built in):
   | Key | Default if unset |
   |---|---|
   | `ALLOWED_ORIGINS` | `https://sahukumarsonu.github.io` |
   | `PASSWORD_PEPPER` | `dev-only-pepper-change-me` (⚠️ set your own for real use) |
   | `BANK_DB_PATH` | `./bank.db` (⚠️ set to `/data/bank.db` + add a Render Disk for persistence — see below) |
3. **Persistent storage:** Render's filesystem resets on every deploy.
   Add a Disk mounted at `/data`, and set `BANK_DB_PATH=/data/bank.db`, or
   all data (including demo users) is wiped on every redeploy.
4. Free-tier note: the service spins down after ~15 min idle, and the
   first request after that can take up to a minute. Hit `/api/health`
   before a demo to warm it up.
5. Verify: `https://internet-banking-ek6f.onrender.com/api/health` should
   return `{"status":"ok","service":"internet-banking-system"}`.

---

## API Reference

Base path `/api`. Authenticated routes need `Authorization: Bearer <token>`.

| Method | Endpoint | Auth | Notes |
|---|---|---|---|
| POST | `/auth/register` | — | Simple request (no preflight) |
| POST | `/auth/login` | — | Simple request (no preflight) |
| POST | `/auth/logout` | Bearer | |
| GET  | `/auth/me` | Bearer | |
| GET  | `/account` | Bearer | |
| GET  | `/account/balance` | Bearer | |
| GET  | `/profile` | Bearer | |
| PUT  | `/profile` | Bearer | |
| PUT  | `/profile/password` | Bearer | |
| POST | `/transactions/deposit` | Bearer | Not auto-retried client-side |
| POST | `/transactions/withdraw` | Bearer | Not auto-retried client-side |
| POST | `/transactions/transfer` | Bearer | Not auto-retried client-side, atomic server-side |
| GET  | `/transactions` | Bearer | `type, from, to, page, pageSize` query params |
| GET  | `/transactions/{id}` | Bearer | |
| POST | `/admin/login` | — | Simple request (no preflight) |
| GET  | `/admin/dashboard` | Bearer (admin) | |
| GET  | `/admin/customers` | Bearer (admin) | `search` query param |
| GET  | `/admin/transactions` | Bearer (admin) | `limit` query param |
| PUT  | `/admin/customers/{id}/status` | Bearer (admin) | |
| GET  | `/health` | — | |
| OPTIONS | `/<anything under /api/>` | — | CORS preflight catch-all |

---

## Security notes

- Passwords: PBKDF2-HMAC-SHA256, 210,000 iterations, per-user random salt,
  server-side pepper (via OpenSSL).
- All SQL uses prepared statements.
- Balances are recalculated and re-checked server-side on every write —
  the frontend never sends or is trusted for a balance value. Withdrawals
  and transfers are guarded by `WHERE balance_paise >= ?` so concurrent
  requests can't overdraw.
- Transfers run inside a single SQLite transaction (`BEGIN`/`COMMIT`) with
  `ROLLBACK` on any failure — both balances move together or neither does.
- Login attempts are rate-limited per email.
- Sessions are random 256-bit server-side tokens with an expiry, not JWTs.

---

## Testing checklist

- [ ] `GET /api/health` → `{"status":"ok",...}`
- [ ] Register a new account (fresh, unused email) → redirected to login
- [ ] Log in → dashboard loads balance + account number
- [ ] Deposit → balance and recent transactions update
- [ ] Withdraw more than balance → clean "insufficient balance" error
- [ ] Transfer between two accounts → both balances update correctly
- [ ] Transaction history search/filter/pagination all work
- [ ] Log out → visiting `dashboard.html` directly redirects to login
- [ ] Admin login (`admin.html`) → dashboard shows totals
- [ ] Deactivating a customer prevents their login

## Future improvements

- Move from SQLite to PostgreSQL for concurrent-write scalability
- Server-side idempotency keys for deposit/withdraw/transfer, so the
  client-side retry restriction described above could be safely lifted
- Refresh tokens instead of one long-lived session token
- Email verification, downloadable statements, 2FA
