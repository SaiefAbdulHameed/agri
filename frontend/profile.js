const API = "http://localhost:8080";

function saveProfile() {
  const email = localStorage.getItem("userEmail");

    const payload = {
        state: document.getElementById("state").value,
        district: document.getElementById("district").value,
        village: document.getElementById("village").value,
        land_size: parseInt(document.getElementById("land_size").value),
        experience: document.getElementById("experience").value
    };

    fetch(API + "/profile/update", {
        method: "POST",
        headers: {
            "Content-Type": "application/json",
            "X-User": email
        },
        body: JSON.stringify(payload)
    })
    .then(r => r.json())
    .then(d => {
        if (d.success) alert("Profile saved");
        else alert("Failed to save profile");
    });
}
