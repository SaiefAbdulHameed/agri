const API = "http://localhost:8080";

function register() {
    const fullname = document.getElementById("fullname")?.value.trim();
    const email = document.getElementById("email")?.value.trim();
    const phone = document.getElementById("phone")?.value.trim();
    const password = document.getElementById("password")?.value;
    const confirmPassword = document.getElementById("confirm_password")?.value;
    const experience = document.getElementById("experience")?.value;
    const state = document.getElementById("state")?.value;
    const district = document.getElementById("district")?.value;
    const village = document.getElementById("village")?.value;
    const land = document.getElementById("land")?.value;
    const roleEl = document.querySelector('input[name="role"]:checked');
    const msg = document.getElementById("msg");

    if (!fullname || !email || !phone || !password || !confirmPassword) {
        showError(msg, "All fields are required");
        return;
    }

    if (password !== confirmPassword) {
        showError(msg, "Passwords do not match");
        return;
    }

    const payload = {
        fullname,
        email,
        phone,
        password,
        state: state || "",
        district: district || "",
        village: village || "",
        land_size: parseFloat(land) || 0,
        experience: experience || "",
        role: roleEl ? roleEl.value : "farmer"
    };

    fetch(API + "/register", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload)
    })
    .then(res => res.json())
    .then(data => {
        if (!data.success) {
            showError(msg, data.message || "Registration failed");
            return;
        }

        showSuccess(msg, "Registration submitted! Please wait for admin approval.");
        
        setTimeout(() => {
            window.location.href = "login.html";
        }, 2000);
    })
    .catch(err => {
        console.error(err);
        showError(msg, "Server error");
    });
}
function login() {
    console.log("[LOGIN] started");

    const email = document.getElementById("email")?.value.trim();
    const password = document.getElementById("password")?.value;
    const msg = document.getElementById("msg");

    if (!email || !password) {
        showError(msg, "Enter email and password");
        return;
    }

    fetch(API + "/login", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ email, password })
    })
    .then(res => res.json())
    .then(data => {
        console.log("[LOGIN] response =", data);

        // Check status FIRST - even if success is false
        if (data.status === "pending") {
            showError(msg, "⏳ Your account is pending admin approval. Please check back later.");
            return;
        }

        if (data.status === "disabled") {
            showError(msg, "🚫 Your account has been disabled. Contact support.");
            return;
        }

        // Now check success
        if (!data.success) {
            showError(msg, data.message || "Invalid email or password");
            return;
        }

        // Success - active account
        document.cookie = "auth=true; path=/";
        localStorage.setItem("user", JSON.stringify(data.user));
        localStorage.setItem("userEmail", data.user?.email || email);

        showSuccess(msg, "✅ Login successful! Redirecting...");
        
        setTimeout(() => {
            window.location.replace("index.html");
        }, 800);
    })
    .catch(err => {
        console.error("[LOGIN] error", err);
        showError(msg, "Server connection failed");
    });
}

function logout() {
    if (!confirm("Are you sure you want to logout?")) return;
    
    sessionStorage.clear();
    localStorage.removeItem("user");
    localStorage.removeItem("userEmail");
    document.cookie = "auth=; Max-Age=0; path=/";
    
    window.location.replace("login.html");
}

function showError(el, text) {
    if (!el) return;
    el.textContent = text;
    el.className = "message error show";
}

function showSuccess(el, text) {
    if (!el) return;
    el.textContent = text;
    el.className = "message success show";
}
