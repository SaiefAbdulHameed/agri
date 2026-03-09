(function () {
    function checkAuth() {
        const isLoggedIn = document.cookie
            .split("; ")
            .some(c => c.startsWith("auth=true"));

        if (!isLoggedIn) {
            console.warn("[GUARD] not authenticated → redirecting");
            window.location.replace("login.html");
        }
    }

    // Normal load
    checkAuth();

    // 🔥 CRITICAL: handles browser back/forward cache
    window.addEventListener("pageshow", function (event) {
        if (event.persisted) {
            console.log("[GUARD] page restored from cache → rechecking auth");
            checkAuth();
        }
    });
})();
