// tracks polling intervals per project — key=name, value=interval ID
var outputPollers = {};

// ── on page load ───────────────────────────────────────────────────────────
document.addEventListener("DOMContentLoaded", function() {
    loadProjects();
});

// ── loadProjects ───────────────────────────────────────────────────────────
// fetches project list from C server and renders cards
function loadProjects() {
    fetch("/api/projects")
        .then(function(r) { return r.json(); })
        .then(function(projects) { renderProjects(projects); })
        .catch(function(e) { console.error("Failed to load projects:", e); });
}

// ── renderProjects ─────────────────────────────────────────────────────────
// builds a card element for each project and injects into the page
function renderProjects(projects) {
    var container = document.getElementById("project-list");
    container.innerHTML = "";

    if (projects.length === 0) {
        container.innerHTML = "<div class='empty-state'>No projects yet. Add one on the right.</div>";
        return;
    }

    projects.forEach(function(project) {
        var card = document.createElement("div");
        card.className = "project-card";
        card.id = "card-" + project.name; // unique ID so we can find it later

        card.innerHTML =
            "<div class='card-header'>" +
                "<h3>" + project.name + "</h3>" +
                "<span class='status stopped' id='status-" + project.name + "'>Stopped</span>" +
            "</div>" +
            "<p class='project-path'>" + project.path + "</p>" +
            "<p class='project-command'>Command: " + project.command + "</p>" +
            "<div class='card-actions'>" +
                "<button class='btn-run' id='btn-" + project.name + "'" +
                    " onclick='runProject(" +
                        JSON.stringify(project.name) + "," +
                        JSON.stringify(project.path) + "," +
                        JSON.stringify(project.command) + "," +
                        project.db_port +
                    ")'>Run</button>" +
            "</div>" +
            // output panel — hidden until project is running
            "<div class='output-panel' id='output-" + project.name + "'></div>";

        container.appendChild(card);
    });
}

// ── runProject ─────────────────────────────────────────────────────────────
// called when Run button is clicked
// sends POST /api/run to the C server, then starts polling for output
function runProject(name, path, command, db_port) {
    var btn = document.getElementById("btn-" + name);
    btn.disabled = true;
    btn.textContent = "Starting...";

    fetch("/api/run", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        // JSON.stringify turns the JS object into a JSON string for the request body
        body: JSON.stringify({ name: name, path: path, command: command, db_port: db_port })
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
        if (data.ok) {
            // project started — update UI and begin polling for output
            setRunning(name, true);
            startPolling(name);
        } else {
            // failed to start — show error message and reset button
            var panel = document.getElementById("output-" + name);
            panel.style.display = "block";
            panel.textContent = "Error: " + (data.error || "Could not start project");
            btn.disabled = false;
            btn.textContent = "Run";
        }
    });
}

// ── stopProject ────────────────────────────────────────────────────────────
// called when Stop button is clicked
// sends POST /api/stop and stops polling
function stopProject(name) {
    fetch("/api/stop", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name: name })
    })
    .then(function(r) { return r.json(); })
    .then(function() {
        stopPolling(name);
        setRunning(name, false);
    });
}

// ── startPolling ───────────────────────────────────────────────────────────
// polls /api/output every 800ms and appends new output to the panel
function startPolling(name) {
    var panel = document.getElementById("output-" + name);
    panel.style.display = "block";
    panel.textContent = "";

    // setInterval calls the function every 800 milliseconds
    // we store the interval ID so we can cancel it later with stopPolling()
    outputPollers[name] = setInterval(function() {
        fetch("/api/output?name=" + encodeURIComponent(name))
            .then(function(r) { return r.json(); })
            .then(function(data) {
                if (data.output && data.output.length > 0) {
                    panel.textContent += data.output; // append new output
                    panel.scrollTop = panel.scrollHeight; // scroll to bottom
                }
                if (!data.running) {
                    // process finished on its own — stop polling and update UI
                    stopPolling(name);
                    setRunning(name, false);
                    panel.textContent += "\n[Process exited]";
                }
            });
    }, 800);
}

// ── stopPolling ────────────────────────────────────────────────────────────
function stopPolling(name) {
    if (outputPollers[name]) {
        clearInterval(outputPollers[name]); // cancel the interval
        delete outputPollers[name];
    }
}

// ── setRunning ─────────────────────────────────────────────────────────────
// updates the status badge and button for a project card
function setRunning(name, running) {
    var status = document.getElementById("status-" + name);
    var btn    = document.getElementById("btn-"    + name);
    if (!status || !btn) return;

    if (running) {
        status.textContent = "Running";
        status.className   = "status running";
        btn.textContent    = "Stop";
        btn.className      = "btn-stop";
        btn.disabled       = false;
        btn.onclick        = function() { stopProject(name); };
    } else {
        status.textContent = "Stopped";
        status.className   = "status stopped";
        btn.textContent    = "Run";
        btn.className      = "btn-run";
        btn.disabled       = false;
        // reset onclick — we need the full project data again
        // reload projects to restore correct onclick with path/command/db_port
        loadProjects();
    }
}

// ── addProject ─────────────────────────────────────────────────────────────
// reads the add form and sends POST /api/add to the C server
function addProject() {
    var name    = document.getElementById("f-name").value.trim();
    var path    = document.getElementById("f-path").value.trim();
    var command = document.getElementById("f-command").value.trim();
    var db_port = parseInt(document.getElementById("f-dbport").value) || 0;
    var status  = document.getElementById("add-status");

    if (!name || !path || !command) {
        status.textContent = "Name, path and command are required.";
        status.style.color = "#ff6b6b";
        return;
    }

    fetch("/api/add", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name: name, path: path, command: command, db_port: db_port })
    })
    .then(function(r) { return r.json(); })
    .then(function(data) {
        if (data.ok) {
            status.textContent = "Project added.";
            status.style.color = "#a8ff78";
            // clear the form
            document.getElementById("f-name").value    = "";
            document.getElementById("f-path").value    = "";
            document.getElementById("f-command").value = "";
            document.getElementById("f-dbport").value  = "0";
            loadProjects(); // refresh the project list
        } else {
            status.textContent = data.error || "Failed to add project.";
            status.style.color = "#ff6b6b";
        }
    });
}