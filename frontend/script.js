function submitForm() {
    console.log("[FORM] submitting soil image");

    const fileInput = document.getElementById("soil_image");
    if (!fileInput.files || fileInput.files.length === 0) {
        alert("Please select a soil image!");
        return;
    }

    const formData = new FormData();
    formData.append("image", fileInput.files[0]);

    fetch("http://localhost:8080/predict", {
        method: "POST",
        body: formData
    })
    .then(res => res.json())
    .then(data => {
        console.log("[FORM] response =", data);

        if (data.success) {
            document.getElementById("soil_type").innerText = "Soil Type: " + data.soil_type;
            document.getElementById("recommended_crops").innerText = "Recommended Crops: " + data.recommended_crops;
            document.getElementById("water_retention").innerText = "Water Retention: " + data.water_retention;
        } else {
            document.getElementById("soil_type").innerText = "Prediction failed";
            document.getElementById("recommended_crops").innerText = "";
            document.getElementById("water_retention").innerText = "";
        }
    })
    .catch(err => {
        console.error("[FORM] fetch error", err);
        document.getElementById("soil_type").innerText = "Server error";
        document.getElementById("recommended_crops").innerText = "";
        document.getElementById("water_retention").innerText = "";
    });
}

document.addEventListener("DOMContentLoaded", () => {
    loadProfile();
});

function loadProfile() {
    const email = localStorage.getItem("user_email");
    if (!email) return;

    fetch("http://localhost:8080/me", {
        headers: { "X-User": email }
    })
    .then(res => res.json())
    .then(d => {
        console.log("[PROFILE] data =", d);

        // Use fallback values if fields are empty
        const fullname = d.fullname || "N/A";
        const role = d.role || "Farmer";
        const village = d.village || "Unknown Village";
        const district = d.district || "Unknown District";
        const state = d.state || "Unknown State";

        // Update profile card
        document.getElementById("profile-name").innerText = fullname;
        document.getElementById("profile-role").innerText = role;

        // Update account info list
        document.getElementById("p-fullname").innerText = fullname;
        document.getElementById("p-location").innerText = `${village}, ${district}, ${state}`;
        document.getElementById("p-role").innerText = role;
    })
    .catch(err => {
        console.error("[PROFILE] fetch error", err);
        // Fallback in case server is unreachable
        document.getElementById("profile-name").innerText = "N/A";
        document.getElementById("profile-role").innerText = "Farmer";
        document.getElementById("p-fullname").innerText = "N/A";
        document.getElementById("p-location").innerText = "Unknown Location";
        document.getElementById("p-role").innerText = "Farmer";
    });
}

document.addEventListener("DOMContentLoaded", () => {
    loadProfile();
});



