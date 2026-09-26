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
    const [account, txData] = await Promise.all([
      Api.get("/api/account"),
      Api.get("/api/transactions", { query: { page: 1, pageSize: 5 } }),
    ]);

    balanceEl.textContent = formatCurrency(account.balance);
    accountEl.textContent = account.accountNumber;

    // Deposits/withdrawals totals: fetch full history counts via a couple of
    // lightweight calls (kept simple for a college-scale dataset).
    const [deposits, withdrawals] = await Promise.all([
      Api.get("/api/transactions", { query: { type: "deposit", pageSize: 1 } }),
      Api.get("/api/transactions", { query: { type: "withdrawal", pageSize: 1 } }),
    ]);
    depositsEl.textContent = deposits.total;
    withdrawalsEl.textContent = withdrawals.total;

    renderRecentTransactions(txData.items, txBody, emptyState);
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
