/**
 * profile.js — view/update profile and change password (profile.html).
 */

function initProfilePage() {
  if (!requireCustomerAuth()) return;
  initAppShell("profile");
  loadProfile();

  document.getElementById("profile-form").addEventListener("submit", onProfileSubmit);
  document.getElementById("password-form").addEventListener("submit", onPasswordSubmit);
}

async function loadProfile() {
  try {
    const profile = await Api.get("/api/profile");
    document.getElementById("profile-fullName").value = profile.fullName;
    document.getElementById("profile-email").value = profile.email;
    document.getElementById("profile-phone").value = profile.phone;
    document.getElementById("profile-account-number").textContent = profile.accountNumber;
    document.getElementById("profile-created-at").textContent = formatDateTime(profile.createdAt);
  } catch (err) {
    showToast(err.message, "error");
  }
}

async function onProfileSubmit(e) {
  e.preventDefault();
  const fullName = document.getElementById("profile-fullName").value.trim();
  const phone = document.getElementById("profile-phone").value.trim();
  document.getElementById("profile-fullName-error").textContent = "";
  document.getElementById("profile-phone-error").textContent = "";

  let valid = true;
  if (!fullName) { document.getElementById("profile-fullName-error").textContent = "Full name is required"; valid = false; }
  if (!/^\+?[0-9]{7,15}$/.test(phone)) { document.getElementById("profile-phone-error").textContent = "Enter a valid phone number"; valid = false; }
  if (!valid) return;

  const btn = document.getElementById("profile-submit");
  btn.disabled = true;
  try {
    await Api.put("/api/profile", { fullName, phone });
    showToast("Profile updated successfully", "success");
    const user = Api.getCurrentUser();
    if (user) { user.fullName = fullName; Api.setSession(Api.getToken(), user); }
  } catch (err) {
    showToast(err.message, "error");
  } finally {
    btn.disabled = false;
  }
}

async function onPasswordSubmit(e) {
  e.preventDefault();
  const currentPassword = document.getElementById("current-password").value;
  const newPassword = document.getElementById("new-password").value;
  const confirmNewPassword = document.getElementById("confirm-new-password").value;

  ["current-password", "new-password", "confirm-new-password"].forEach((id) => {
    document.getElementById(id + "-error").textContent = "";
  });

  let valid = true;
  if (!currentPassword) { document.getElementById("current-password-error").textContent = "Required"; valid = false; }
  if (newPassword.length < 8) { document.getElementById("new-password-error").textContent = "At least 8 characters"; valid = false; }
  if (newPassword !== confirmNewPassword) { document.getElementById("confirm-new-password-error").textContent = "Passwords do not match"; valid = false; }
  if (!valid) return;

  const btn = document.getElementById("password-submit");
  btn.disabled = true;
  try {
    await Api.put("/api/profile/password", { currentPassword, newPassword });
    showToast("Password changed successfully", "success");
    document.getElementById("password-form").reset();
  } catch (err) {
    showToast(err.message, "error");
  } finally {
    btn.disabled = false;
  }
}
