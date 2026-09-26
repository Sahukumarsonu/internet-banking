/**
 * transactions.js — transaction history page (search, filter, paginate).
 */

let currentPage = 1;
const pageSize = 10;

function initTransactionsPage() {
  if (!requireCustomerAuth()) return;
  initAppShell("transactions");

  document.getElementById("filter-form").addEventListener("submit", (e) => {
    e.preventDefault();
    currentPage = 1;
    loadTransactions();
  });
  document.getElementById("filter-reset").addEventListener("click", () => {
    document.getElementById("filter-form").reset();
    currentPage = 1;
    loadTransactions();
  });
  document.getElementById("prev-page").addEventListener("click", () => {
    if (currentPage > 1) { currentPage--; loadTransactions(); }
  });
  document.getElementById("next-page").addEventListener("click", () => {
    currentPage++; loadTransactions();
  });

  loadTransactions();
}

async function loadTransactions() {
  const tbody = document.getElementById("tx-body");
  const emptyState = document.getElementById("tx-empty");
  const tableWrap = document.getElementById("tx-table-wrap");
  const search = document.getElementById("filter-search").value.trim().toLowerCase();
  const type = document.getElementById("filter-type").value;
  const from = document.getElementById("filter-from").value;
  const to = document.getElementById("filter-to").value;

  tbody.innerHTML = `<tr><td colspan="5" class="loading-row"><span class="spinner spinner-dark"></span> Loading...</td></tr>`;
  emptyState.style.display = "none";
  tableWrap.style.display = "block";

  try {
    const data = await Api.get("/api/transactions", {
      query: { type, from, to, page: currentPage, pageSize },
    });

    let items = data.items;
    if (search) {
      items = items.filter(
        (t) =>
          (t.description || "").toLowerCase().includes(search) ||
          (t.counterpartyAccount || "").toLowerCase().includes(search)
      );
    }

    tbody.innerHTML = "";
    if (!items || items.length === 0) {
      tableWrap.style.display = "none";
      emptyState.style.display = "block";
    } else {
      items.forEach((tx) => {
        const isCredit = tx.type === "deposit" || tx.type === "transfer_in";
        const tr = document.createElement("tr");
        tr.innerHTML = `
          <td>#${tx.id}</td>
          <td><span class="badge badge-${tx.type}">${formatTxType(tx.type)}</span></td>
          <td>${tx.description || "-"}${tx.counterpartyAccount ? " · " + tx.counterpartyAccount : ""}</td>
          <td class="${isCredit ? "amount-positive" : "amount-negative"}">${isCredit ? "+" : "-"}${formatCurrency(tx.amount)}</td>
          <td><span class="badge badge-${tx.status}">${tx.status}</span></td>
          <td>${formatDateTime(tx.createdAt)}</td>
        `;
        tbody.appendChild(tr);
      });
    }

    document.getElementById("page-info").textContent = `Page ${data.page} of ${Math.max(1, Math.ceil(data.total / data.pageSize))}`;
    document.getElementById("prev-page").disabled = currentPage <= 1;
    document.getElementById("next-page").disabled = currentPage >= Math.ceil(data.total / data.pageSize);
  } catch (err) {
    tbody.innerHTML = "";
    showToast(err.message, "error");
  }
}
