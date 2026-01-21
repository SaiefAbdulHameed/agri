const API = "http://localhost:8080";

function register() {
    const fullname = document.getElementById("fullname").value.trim();
    const email = document.getElementById("email").value.trim();
    const state = document.getElementById("state").value.trim();
    const district = document.getElementById("district").value.trim();
    const village = document.getElementById("village").value.trim();
    const land = document.getElementById("land").value;
    const soil_method = document.getElementById("soil_method").value;
    const experience = document.getElementById("experience").value;
    const role = document.getElementById("role").value;
    const password = document.getElementById("password").value;
    const msg = document.getElementById("msg");

    // Basic validation
    if (!fullname || !email || !state || !district || !village || !password) {
        msg.innerText = "Please fill all required fields";
        return;
    }

    fetch("http://localhost:8080/register", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
            fullname,
            email,
            state,
            district,
            village,
            land_size: land,
            soil_method,
            experience,
            role,
            password
        })
    })
    .then(res => res.json())
    .then(data => {
        msg.innerText = data.message;
    })
    .catch(() => {
        msg.innerText = "Server error";
    });
}


function login() {
    fetch(API + "/login", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
            email: email.value,
            password: password.value
        })
    })
    .then(r => r.json())
    .then(d => {
        if (!d.success) return msg.innerText = d.message;

        localStorage.setItem("user", JSON.stringify(d.user));

        if (d.user.role === "admin")
            window.location.href = "admin.html";
        else
            window.location.href = "index.html"; // FARMER DASHBOARD
    });
}
