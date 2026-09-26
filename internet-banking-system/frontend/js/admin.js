/**
 * admin.js — admin login page and admin dashboard/customers/transactions.
 */

function initAdminLoginPage() {
  const user = Api.getCurrentUser();
  if (Api.isLoggedIn() && user && user.isAdmin) {
    window.location.href = "admin-dashboard.html";
    return;
  }

  const params = new URLSearchParams(window.location.search);
  if (params.get("expired") === "1") {
    showToast("Your session expired. Please log in again.", "info");
  }

  const form = document.getElementById("admin-login-form");
  const alertBox = document.getElementById("admin-login-alert");
  const submitBtn = document.getElementById("admin-login-submit");

  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    alertBox.style.display = "none";
    const email = document.getElementById("admin-email").value.trim();
    const password = document.getElementById("admin-password").value;

    submitBtn.disabled = true;
    submitBtn.innerHTML = '<span class="spinner"></span> Logging in...';
    try {
      const data = await Api.post("/api/admin/login", { email, password }, { auth: false });
      Api.setSession(data.token, data.user);
      window.location.href = "admin-dashboard.html";
    } catch (err) {
      alertBox.textContent = err.message;
      alertBox.style.display = "block";
    } finally {
      submitBtn.disabled = false;
      submitBtn.textContent = "Admin Log In";
    }
  });
}

async function initAdminDashboardPage() {
  if (!requireAdminAuth()) return;
  initAppShell("admin", "admin.html");

  await Promise.all([loadAdminStats(), loadCustomers(), loadAdminTransactions()]);

  document.getElementById("admin-search-form").addEventListener("submit", (e) => {
    e.preventDefault();
    loadCustomers();
  });
}

async function loadAdminStats() {
  try {
    const stats = await Api.get("/api/admin/dashboard");
    document.getElementById("stat-total-customers").textContent = stats.totalCustomers;
    document.getElementById("stat-total-accounts").textContent = stats.totalAccounts;
    document.getElementById("stat-total-transactions").textContent = stats.totalTransactions;
    document.getElementById("stat-total-deposits-held").textContent = formatCurrency(stats.totalDepositsHeld);
  } catch (err) {
    showToast(err.message, "error");
  }
}

async function loadCustomers() {
  const tbody = document.getElementById("customers-body");
  const search = document.getElementById("admin-search").value.trim();
  tbody.innerHTML = `<tr><td colspan="6" class="loading-row"><span class="spinner spinner-dark"></span> Loading...</td></tr>`;
  try {
    const data = await Api.get("/api/admin/customers", { query: { search } });
    tbody.innerHTML = "";
    if (!data.customers || data.customers.length === 0) {
      tbody.innerHTML = `<tr><td colspan="6" class="loading-row">No customers found.</td></tr>`;
      return;
    }
    data.customers.forEach((c) => {
      const tr = document.createElement("tr");
      tr.innerHTML = `
        <td>${c.fullName}</td>
        <td>${c.email}</td>
        <td>${c.accountNumber}</td>
        <td>${formatCurrency(c.balance)}</td>
        <td><span class="badge badge-${c.isActive ? "active" : "inactive"}">${c.isActive ? "Active" : "Deactivated"}</span></td>
        <td>
          <button class="btn btn-sm ${c.isActive ? "btn-danger" : "btn-secondary"}" data-toggle-id="${c.id}" data-active="${c.isActive}">
            ${c.isActive ? "Deactivate" : "Activate"}
          </button>
        </td>
      `;
      tbody.appendChild(tr);
    });

    tbody.querySelectorAll("[data-toggle-id]").forEach((btn) => {
      btn.addEventListener("click", () => toggleCustomerStatus(btn));
    });
  } catch (err) {
    tbody.innerHTML = "";
    showToast(err.message, "error");
  }
}

async function toggleCustomerStatus(btn) {
  const id = btn.dataset.toggleId;
  const currentlyActive = btn.dataset.active === "true";
  btn.disabled = true;
  try {
    await Api.put(`/api/admin/customers/${id}/status`, { isActive: !currentlyActive });
    showToast(`Customer ${!currentlyActive ? "activated" : "deactivated"}`, "success");
    loadCustomers();
    loadAdminStats();
  } catch (err) {
    showToast(err.message, "error");
    btn.disabled = false;
  }
}

async function loadAdminTransactions() {
  const tbody = document.getElementById("admin-tx-body");
  tbody.innerHTML = `<tr><td colspan="6" class="loading-row"><span class="spinner spinner-dark"></span> Loading...</td></tr>`;
  try {
    const data = await Api.get("/api/admin/transactions", { query: { limit: 50 } });
    tbody.innerHTML = "";
    if (!data.transactions || data.transactions.length === 0) {
      tbody.innerHTML = `<tr><td colspan="6" class="loading-row">No transactions yet.</td></tr>`;
      return;
    }
    data.transactions.forEach((tx) => {
      const isCredit = tx.type === "deposit" || tx.type === "transfer_in";
      const tr = document.createElement("tr");
      tr.innerHTML = `
        <td>#${tx.id}</td>
        <td>${tx.customerName}</td>
        <td><span class="badge badge-${tx.type}">${formatTxType(tx.type)}</span></td>
        <td class="${isCredit ? "amount-positive" : "amount-negative"}">${formatCurrency(tx.amount)}</td>
        <td><span class="badge badge-${tx.status}">${tx.status}</span></td>
        <td>${formatDateTime(tx.createdAt)}</td>
      `;
      tbody.appendChild(tr);
    });
  } catch (err) {
    tbody.innerHTML = "";
    showToast(err.message, "error");
  }
}
