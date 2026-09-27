/**
 * api.js — single configurable place for the backend API base URL,
 * plus a small fetch wrapper with auth, JSON handling, retry, and error
 * messages.
 *
 * PRODUCTION_API_URL is hardcoded to this project's actual deployed
 * backend. When running locally (localhost/127.0.0.1), it automatically
 * falls back to a local backend on port 8080 instead — no manual editing
 * needed either way.
 */
const PRODUCTION_API_URL = "https://internet-banking-ek6f.onrender.com";

const API_BASE_URL = (function () {
  if (window.location.hostname === "localhost" || window.location.hostname === "127.0.0.1") {
    return "http://localhost:8080";
  }
  return PRODUCTION_API_URL;
})();

const TOKEN_KEY = "ibs_token";
const USER_KEY = "ibs_user";

const Api = {
  baseUrl: API_BASE_URL,

  getToken() {
    return localStorage.getItem(TOKEN_KEY);
  },
  setSession(token, user) {
    localStorage.setItem(TOKEN_KEY, token);
    localStorage.setItem(USER_KEY, JSON.stringify(user));
  },
  clearSession() {
    localStorage.removeItem(TOKEN_KEY);
    localStorage.removeItem(USER_KEY);
  },
  getCurrentUser() {
    const raw = localStorage.getItem(USER_KEY);
    return raw ? JSON.parse(raw) : null;
  },
  isLoggedIn() {
    return !!this.getToken();
  },

  /**
   * Core request helper. Throws an Error with a human-readable message
   * on failure (network error, non-2xx status, etc.) so callers can just
   * try/catch and show err.message to the user.
   *
   * skipPreflight: when true, sends the JSON body with a "simple" request
   * Content-Type (text/plain) instead of application/json, which makes
   * the browser skip its CORS preflight (OPTIONS) request entirely for
   * this call. The backend parses the raw body as JSON regardless of what
   * Content-Type header says, so this only changes what the browser does,
   * not what the server does. Only safe for endpoints that don't need the
   * Authorization header — that header alone always forces a preflight no
   * matter what Content-Type is used, so it's used for login/register/
   * admin-login only.
   */
  async request(path, { method = "GET", body, auth = true, query, skipPreflight = false } = {}) {
    let url = this.baseUrl + path;
    if (query) {
      const qs = new URLSearchParams(
        Object.entries(query).filter(([, v]) => v !== undefined && v !== null && v !== "")
      ).toString();
      if (qs) url += "?" + qs;
    }

    const headers = { "Content-Type": skipPreflight ? "text/plain" : "application/json" };
    if (auth) {
      const token = this.getToken();
      if (token) headers["Authorization"] = "Bearer " + token;
    }

    let response;
    try {
      response = await fetch(url, {
        method,
        headers,
        body: body !== undefined ? JSON.stringify(body) : undefined,
      });
    } catch (networkErr) {
      // Tagged so callers (see requestWithRetry below) can tell "never
      // reached the server, or the response got lost in transit" apart
      // from a real HTTP error response the server chose to send (like a
      // 409). This matters because free-tier hosting occasionally drops a
      // response after the server has already processed the request — so
      // a network-level failure here does NOT mean the request didn't
      // happen.
      const err = new Error(
        "Could not reach the banking server. It may be waking up from idle (free-tier hosting " +
        "sleeps after inactivity) — this can take up to a minute on the first request. " +
        "Please try again in a moment."
      );
      err.isNetworkFailure = true;
      throw err;
    }

    let data = null;
    const text = await response.text();
    if (text) {
      try { data = JSON.parse(text); } catch (e) { /* non-JSON response */ }
    }

    if (response.status === 401 && auth) {
      // Session expired or invalid — clear it and send the user back to the
      // right login screen (admin pages go back to the admin login).
      this.clearSession();
      const path = window.location.pathname;
      const onAdminPage = path.endsWith("admin.html") || path.endsWith("admin-dashboard.html");
      const alreadyOnLogin = path.endsWith("login.html") || path.endsWith("admin.html") || path.endsWith("index.html");
      if (!alreadyOnLogin) {
        window.location.href = (onAdminPage ? "admin.html" : "login.html") + "?expired=1";
      }
    }

    if (!response.ok) {
      const message = (data && data.error) ? data.error : `Request failed (HTTP ${response.status})`;
      const err = new Error(message);
      err.status = response.status;
      throw err;
    }

    return data;
  },

  get(path, opts) { return this.request(path, { ...opts, method: "GET" }); },
  post(path, body, opts) { return this.request(path, { ...opts, method: "POST", body }); },
  put(path, body, opts) { return this.request(path, { ...opts, method: "PUT", body }); },

  /**
   * Same as request(), but if the first attempt fails with a network-level
   * failure (isNetworkFailure), waits briefly and tries exactly once more
   * before giving up. Free-tier hosting (both the frontend and backend
   * here) can occasionally drop a request or response in transit even
   * when the server processed it correctly — a same-request retry either
   * gets the real answer this time, or comes back with a clear, specific
   * error (like "already exists") that's more honest to the user than a
   * generic connection error when the action may have actually succeeded.
   * Only retries once, and only for network failures — a real HTTP error
   * response (400, 401, 409, ...) is never retried or hidden.
   */
  async requestWithRetry(path, opts) {
    try {
      return await this.request(path, opts);
    } catch (err) {
      if (!err.isNetworkFailure) throw err;
      await new Promise((resolve) => setTimeout(resolve, 1500));
      try {
        return await this.request(path, opts);
      } catch (retryErr) {
        // Tag so the caller knows this came from the retry, not the first
        // attempt — useful for telling the user "this might be your own
        // request from a moment ago" rather than a plain error.
        retryErr.afterNetworkFailureRetry = true;
        throw retryErr;
      }
    }
  },

  getWithRetry(path, opts) { return this.requestWithRetry(path, { ...opts, method: "GET" }); },
  postWithRetry(path, body, opts) { return this.requestWithRetry(path, { ...opts, method: "POST", body }); },
  putWithRetry(path, body, opts) { return this.requestWithRetry(path, { ...opts, method: "PUT", body }); },
};

/** Requires a logged-in customer; redirects to login otherwise. Call at the top of protected pages. */
function requireCustomerAuth() {
  if (!Api.isLoggedIn()) {
    window.location.href = "login.html";
    return false;
  }
  return true;
}

/** Requires a logged-in admin; redirects otherwise. */
function requireAdminAuth() {
  const user = Api.getCurrentUser();
  if (!Api.isLoggedIn() || !user || !user.isAdmin) {
    window.location.href = "admin.html";
    return false;
  }
  return true;
}

/** Small toast/notification helper shared across pages. */
function showToast(message, type = "info") {
  let container = document.getElementById("toast-container");
  if (!container) {
    container = document.createElement("div");
    container.id = "toast-container";
    document.body.appendChild(container);
  }
  const toast = document.createElement("div");
  toast.className = `toast toast-${type}`;
  toast.textContent = message;
  container.appendChild(toast);
  setTimeout(() => toast.remove(), 4500);
}

/** Formats paise-free rupee amounts consistently, e.g. 12500.5 -> "₹12,500.50" */
function formatCurrency(amount) {
  const n = Number(amount) || 0;
  return "₹" + n.toLocaleString("en-IN", { minimumFractionDigits: 2, maximumFractionDigits: 2 });
}

function formatDateTime(isoLike) {
  if (!isoLike) return "";
  // SQLite datetime('now') gives "YYYY-MM-DD HH:MM:SS" (UTC); make it parseable.
  const iso = isoLike.includes("T") ? isoLike : isoLike.replace(" ", "T") + "Z";
  const d = new Date(iso);
  if (isNaN(d.getTime())) return isoLike;
  return d.toLocaleString("en-IN", { dateStyle: "medium", timeStyle: "short" });
}

function formatTxType(type) {
  return {
    deposit: "Deposit",
    withdrawal: "Withdrawal",
    transfer_out: "Transfer Sent",
    transfer_in: "Transfer Received",
  }[type] || type;
}

/** Wires up the shared sidebar (active link + logout + mobile toggle). Call on DOMContentLoaded.
 *  logoutRedirect lets admin pages send the admin back to the admin login screen. */
function initAppShell(activePage, logoutRedirect) {
  document.querySelectorAll(`.sidebar-nav a[data-page]`).forEach((a) => {
    if (a.dataset.page === activePage) a.classList.add("active");
  });
  const user = Api.getCurrentUser();
  const nameEl = document.getElementById("sidebar-user-name");
  if (nameEl && user) nameEl.textContent = user.fullName;

  const logoutBtns = document.querySelectorAll("[data-action='logout']");
  logoutBtns.forEach((btn) =>
    btn.addEventListener("click", async () => {
      try { await Api.post("/api/auth/logout", {}); } catch (e) { /* ignore */ }
      Api.clearSession();
      window.location.href = logoutRedirect || "login.html";
    })
  );

  const hamburger = document.getElementById("hamburger");
  const sidebar = document.querySelector(".sidebar");
  if (hamburger && sidebar) {
    hamburger.addEventListener("click", () => sidebar.classList.toggle("open"));
  }
}
