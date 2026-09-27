/**
 * dashboard.js — customer dashboard page logic.
 */

async function initDashboardPage() {
  if (!requireCustomerAuth()) return;
  initAppShell("dashboard");

  const balanceEl = document.getElementById("stat-balance");
  const accountEl = document.getElementById("stat-account");
  const depositsEl = document.getElementById("stat-deposits");
  const withdrawalsEl = document.getElementById("stat-withdrawals");
  const nameEl = document.getElementById("welcome-name");
  const txBody = document.getElementById("recent-tx-body");
  const emptyState = document.getElementById("recent-tx-empty");

  const user = Api.getCurrentUser();
  if (nameEl && user) nameEl.textContent = user.fullName.split(" ")[0];

  try {
    // Fired sequentially, not via Promise.all — Render's free-tier edge has
    // shown a pattern of failing specifically when multiple authenticated
    // requests (each needing its own CORS preflight) hit it at the same
    // instant. One request at a time is slightly slower but noticeably
    // more reliable on that hosting tier.
    const account = await Api.getWithRetry("/api/account");
    balanceEl.textContent = formatCurrency(account.balance);
    accountEl.textContent = account.accountNumber;

    const txData = await Api.getWithRetry("/api/transactions", { query: { page: 1, pageSize: 5 } });
    renderRecentTransactions(txData.items, txBody, emptyState);

    const deposits = await Api.getWithRetry("/api/transactions", { query: { type: "deposit", pageSize: 1 } });
    depositsEl.textContent = deposits.total;

    const withdrawals = await Api.getWithRetry("/api/transactions", { query: { type: "withdrawal", pageSize: 1 } });
    withdrawalsEl.textContent = withdrawals.total;
  } catch (err) {
    showToast(err.message, "error");
  }
}

function renderRecentTransactions(items, tbody, emptyState) {
  tbody.innerHTML = "";
  if (!items || items.length === 0) {
    emptyState.style.display = "block";
    document.getElementById("recent-tx-table-wrap").style.display = "none";
    return;
  }
  emptyState.style.display = "none";
  document.getElementById("recent-tx-table-wrap").style.display = "block";

  items.forEach((tx) => {
    const tr = document.createElement("tr");
    const isCredit = tx.type === "deposit" || tx.type === "transfer_in";
    tr.innerHTML = `
      <td><span class="badge badge-${tx.type}">${formatTxType(tx.type)}</span></td>
      <td>${tx.description || "-"}</td>
      <td class="${isCredit ? "amount-positive" : "amount-negative"}">${isCredit ? "+" : "-"}${formatCurrency(tx.amount)}</td>
      <td>${formatDateTime(tx.createdAt)}</td>
    `;
    tbody.appendChild(tr);
  });
}
