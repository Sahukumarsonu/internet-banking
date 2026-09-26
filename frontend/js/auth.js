/**
 * auth.js — login and registration page logic.
 */

function setFieldError(fieldId, message) {
  const el = document.getElementById(fieldId + "-error");
  if (el) el.textContent = message || "";
}

function clearFieldErrors(ids) {
  ids.forEach((id) => setFieldError(id, ""));
}

function initLoginPage() {
  if (Api.isLoggedIn()) {
    window.location.href = "dashboard.html";
    return;
  }

  const params = new URLSearchParams(window.location.search);
  if (params.get("expired") === "1") {
    showToast("Your session expired. Please log in again.", "info");
  }
  if (params.get("registered") === "1") {
    showToast("Registration successful! Please log in.", "success");
  }

  const form = document.getElementById("login-form");
  const submitBtn = document.getElementById("login-submit");
  const alertBox = document.getElementById("login-alert");

  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    alertBox.style.display = "none";
    clearFieldErrors(["email", "password"]);

    const email = document.getElementById("email").value.trim();
    const password = document.getElementById("password").value;

    let valid = true;
    if (!email) { setFieldError("email", "Email is required"); valid = false; }
    if (!password) { setFieldError("password", "Password is required"); valid = false; }
    if (!valid) return;

    submitBtn.disabled = true;
    submitBtn.innerHTML = '<span class="spinner"></span> Logging in...';

    try {
      const data = await Api.post("/api/auth/login", { email, password }, { auth: false });
      Api.setSession(data.token, data.user);
      window.location.href = "dashboard.html";
    } catch (err) {
      alertBox.textContent = err.message;
      alertBox.style.display = "block";
    } finally {
      submitBtn.disabled = false;
      submitBtn.textContent = "Log In";
    }
  });
}

function initRegisterPage() {
  if (Api.isLoggedIn()) {
    window.location.href = "dashboard.html";
    return;
  }

  const form = document.getElementById("register-form");
  const submitBtn = document.getElementById("register-submit");
  const alertBox = document.getElementById("register-alert");

  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    alertBox.style.display = "none";
    const fields = ["fullName", "email", "phone", "password", "confirmPassword"];
    clearFieldErrors(fields);

    const values = {};
    fields.forEach((f) => { values[f] = document.getElementById(f).value.trim(); });

    let valid = true;
    if (!values.fullName) { setFieldError("fullName", "Full name is required"); valid = false; }
    if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(values.email)) {
      setFieldError("email", "Enter a valid email address"); valid = false;
    }
    if (!/^\+?[0-9]{7,15}$/.test(values.phone)) {
      setFieldError("phone", "Enter a valid phone number (digits only)"); valid = false;
    }
    if (values.password.length < 8) {
      setFieldError("password", "Password must be at least 8 characters"); valid = false;
    }
    if (values.password !== values.confirmPassword) {
      setFieldError("confirmPassword", "Passwords do not match"); valid = false;
    }
    if (!valid) return;

    submitBtn.disabled = true;
    submitBtn.innerHTML = '<span class="spinner"></span> Creating account...';

    try {
      await Api.post("/api/auth/register", values, { auth: false });
      window.location.href = "login.html?registered=1";
    } catch (err) {
      alertBox.textContent = err.message;
      alertBox.style.display = "block";
    } finally {
      submitBtn.disabled = false;
      submitBtn.textContent = "Create Account";
    }
  });
}
