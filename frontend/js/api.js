/**
 * api.js — single configurable place for the backend API base URL,
 * plus a small fetch wrapper with auth, JSON handling, and error messages.
 *
 * DEVELOPMENT: leave API_BASE_URL as "http://localhost:8080"
 * PRODUCTION:  change API_BASE_URL to your deployed Render URL, e.g.
 *              "https://internet-banking-backend.onrender.com"
 * This is the ONLY line you need to edit when deploying the frontend.
 */
const API_BASE_URL = (function () {
  // If this file is served from GitHub Pages, default to the placeholder
  // Render URL below (edit after you deploy the backend). If served from
  // localhost, default to a local backend on port 8080.
  if (window.location.hostname === "localhost" || window.location.hostname === "127.0.0.1") {
    return "http://localhost:8080";
  }
  return "https://YOUR-RENDER-SERVICE.onrender.com"; // <-- EDIT AFTER DEPLOYING BACKEND
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
   */
  async request(path, { method = "GET", body, auth = true, query } = {}) {
    let url = this.baseUrl + path;
    if (query) {
      const qs = new URLSearchParams(
        Object.entries(query).filter(([, v]) => v !== undefined && v !== null && v !== "")
      ).toString();
      if (qs) url += "?" + qs;
    }

    const headers = { "Content-Type": "application/json" };
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
      throw new Error(
        "Could not reach the banking server. Check your connection, or the API_BASE_URL " +
        "configured in js/api.js (" + this.baseUrl + ")."
      );
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
      throw new Error(message);
    }

    return data;
  },

  get(path, opts) { return this.request(path, { ...opts, method: "GET" }); },
  post(path, body, opts) { return this.request(path, { ...opts, method: "POST", body }); },
  put(path, body, opts) { return this.request(path, { ...opts, method: "PUT", body }); },
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
