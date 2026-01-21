function submitForm() {
    const location = document.getElementById("location").value;
    const ph = parseFloat(document.getElementById("ph").value);
    const nitrogen = parseInt(document.getElementById("nitrogen").value);
    const phosphorus = parseInt(document.getElementById("phosphorus").value);
    const potassium = parseInt(document.getElementById("potassium").value);
    const moisture = parseInt(document.getElementById("moisture").value);

    if (!location || isNaN(ph) || isNaN(nitrogen) || isNaN(phosphorus) || isNaN(potassium) || isNaN(moisture)) {
        alert("Please fill all fields correctly!");
        return;
    }

    const payload = `${location},${ph},${nitrogen},${phosphorus},${potassium},${moisture}`;

    fetch("http://localhost:8080", {
        method: "POST",
        headers: { "Content-Type": "text/plain" },
        body: payload
    })
    .then(res => res.json())
    .then(data => {
        // 👇 SHOW RESULT ON PAGE (NOT CONSOLE)
        document.getElementById("crop").innerText =
            "Recommended Crop: " + data.crop;

        document.getElementById("fertilizer").innerText =
            "Urea Price: ₹" + data.ureaPrice;
    })
    .catch(err => {
        document.getElementById("crop").innerText = "Error getting result";
        document.getElementById("fertilizer").innerText = "";
    });
}
