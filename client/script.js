const form = document.getElementById("formulaire");

form.addEventListener("submit", function(e) {
    e.preventDefault();

    localStorage.setItem("id", document.getElementById("input id"));
    localStorage.setItem("password", document.getElementById("input password"));

    window.location.href = "./pages/getsold.html";
});