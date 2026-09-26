/**
 * transfer.js — deposit, withdrawal, and money-transfer forms (transfer.html).
 */

let pendingAction = null; // { type: 'deposit'|'withdraw'|'transfer', payload }

function initTransferPage() {
  if (!requireCustomerAuth()) return;
  initAppShell("transfer");

  loadBalanceHeader();

  document.getElementById("deposit-form").addEventListener("submit", onDepositSubmit);
  document.getElementById("withdraw-form").addEventListener("submit", onWithdrawSubmit);
  document.getElementById("transfer-form").addEventListener("submit", onTransferSubmit);

  document.getElementById("modal-cancel").addEventListener("click", closeModal);
  document.getElementById("modal-confirm").addEventListener("click", onModalConfirm);
}

async function loadBalanceHeader() {
  try {
    const account = await Api.get("/api/account");
    document.getElementById("current-balance").textContent = formatCurrency(account.balance);
    const accEl = document.getElementById("current-account-number");
    if (accEl) accEl.textContent = account.accountNumber;
  } catch (err) {
    showToast(err.message, "error");
  }
}

function openConfirmModal(title, message, onConfirm) {
  document.getElementById("modal-title").textContent = title;
  document.getElementById("modal-message").textContent = message;
  document.getElementById("modal-overlay").classList.remove("hidden");
  pendingAction = onConfirm;
}

function closeModal() {
  document.getElementById("modal-overlay").classList.add("hidden");
  pendingAction = null;
}

async function onModalConfirm() {
  if (typeof pendingAction === "function") {
    const fn = pendingAction;
    closeModal();
    await fn();
  }
}

function onDepositSubmit(e) {
  e.preventDefault();
  const amountInput = document.getElementById("deposit-amount");
  const amount = parseFloat(amountInput.value);
  const description = document.getElementById("deposit-description").value.trim();
  document.getElementById("deposit-amount-error").textContent = "";

  if (!amount || amount <= 0) {
    document.getElementById("deposit-amount-error").textContent = "Enter a valid amount greater than 0";
    return;
  }

  openConfirmModal(
    "Confirm Deposit",
    `Deposit ${formatCurrency(amount)} into your account?`,
    async () => {
      const btn = document.getElementById("deposit-submit");
      btn.disabled = true;
      try {
        const res = await Api.post("/api/transactions/deposit", { amount, description });
        showToast("Deposit successful. New balance: " + formatCurrency(res.newBalance), "success");
        document.getElementById("deposit-form").reset();
        loadBalanceHeader();
      } catch (err) {
        showToast(err.message, "error");
      } finally {
        btn.disabled = false;
      }
    }
  );
}

function onWithdrawSubmit(e) {
  e.preventDefault();
  const amount = parseFloat(document.getElementById("withdraw-amount").value);
  const description = document.getElementById("withdraw-description").value.trim();
  document.getElementById("withdraw-amount-error").textContent = "";

  if (!amount || amount <= 0) {
    document.getElementById("withdraw-amount-error").textContent = "Enter a valid amount greater than 0";
    return;
  }

  openConfirmModal(
    "Confirm Withdrawal",
    `Withdraw ${formatCurrency(amount)} from your account?`,
    async () => {
      const btn = document.getElementById("withdraw-submit");
      btn.disabled = true;
      try {
        const res = await Api.post("/api/transactions/withdraw", { amount, description });
        showToast("Withdrawal successful. New balance: " + formatCurrency(res.newBalance), "success");
        document.getElementById("withdraw-form").reset();
        loadBalanceHeader();
      } catch (err) {
        showToast(err.message, "error");
      } finally {
        btn.disabled = false;
      }
    }
  );
}

function onTransferSubmit(e) {
  e.preventDefault();
  const receiverAccountNumber = document.getElementById("transfer-receiver").value.trim();
  const amount = parseFloat(document.getElementById("transfer-amount").value);
  const description = document.getElementById("transfer-description").value.trim();

  document.getElementById("transfer-receiver-error").textContent = "";
  document.getElementById("transfer-amount-error").textContent = "";

  let valid = true;
  const myAccount = document.getElementById("current-account-number")?.textContent?.trim();
  if (!receiverAccountNumber) {
    document.getElementById("transfer-receiver-error").textContent = "Receiver account number is required";
    valid = false;
  } else if (myAccount && receiverAccountNumber === myAccount) {
    document.getElementById("transfer-receiver-error").textContent = "You cannot transfer to your own account";
    valid = false;
  }
  if (!amount || amount <= 0) {
    document.getElementById("transfer-amount-error").textContent = "Enter a valid amount greater than 0";
    valid = false;
  }
  if (!valid) return;

  openConfirmModal(
    "Confirm Transfer",
    `Transfer ${formatCurrency(amount)} to account ${receiverAccountNumber}?`,
    async () => {
      const btn = document.getElementById("transfer-submit");
      btn.disabled = true;
      try {
        const res = await Api.post("/api/transactions/transfer", {
          receiverAccountNumber, amount, description,
        });
        showToast(`Transfer successful. New balance: ${formatCurrency(res.newBalance)}`, "success");
        document.getElementById("transfer-form").reset();
        loadBalanceHeader();
      } catch (err) {
        showToast(err.message, "error");
      } finally {
        btn.disabled = false;
      }
    }
  );
}
