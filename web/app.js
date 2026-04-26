// runs when the page finishes loading
document.addEventListener("DOMContentLoaded", function() {
    loadProjects();
});

// fetches project list from the C server and draws cards on the page
function loadProjects() {
    fetch("/api/projects")  // sends GET /api/projects to the C server
        .then(function(response) {
            return response.json(); // parse the response body as JSON
        })
        .then(function(projects) {
            renderProjects(projects); // draw the cards
        })
        .catch(function(err) {
            console.error("Failed to load projects:", err);
        });
}

// takes the project array and builds HTML cards for each one
function renderProjects(projects) {
    var container = document.querySelector(".container");

    if (projects.length === 0) {
        // nothing in the list — show the empty state message
        container.innerHTML = "<div class='project-card'><h3>No projects yet</h3><p>Projects will appear here once registered.</p></div>";
        return;
    }

    // clear the container before drawing
    container.innerHTML = "";

    // loop through each project and create a card for it
    projects.forEach(function(project) {
        var card = document.createElement("div");
        card.className = "project-card";

        // build the card HTML — name, path, run button
        card.innerHTML =
            "<div class='card-header'>" +
                "<h3>" + project.name + "</h3>" +
                "<span class='status stopped'>Stopped</span>" +
            "</div>" +
            "<p class='project-path'>" + project.path + "</p>" +
            "<p class='project-command'>Command: " + project.command + "</p>" +
            "<div class='card-actions'>" +
                "<button class='btn-run' onclick='runProject(\"" + project.name + "\")'>Run</button>" +
            "</div>";

        container.appendChild(card); // add the card to the page
    });
}

// called when Run button is clicked — wired up in Day 3
function runProject(name) {
    console.log("Run clicked for:", name);
    // Day 3 will send POST /api/run here
}