#include <winsock2.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "router.h"
#include "projects.h"
#include "runner.h"

void send_file(int client_fd, const char *filepath, const char *content_type);
void send_json(int client_fd, const char *json);
void send_404(int client_fd);

// ── parse_body ─────────────────────────────────────────────────────────────
// finds the body of a POST request — the part after the blank line \r\n\r\n
// returns pointer into buffer where body starts, or NULL if not found
static const char *parse_body(const char *buffer) {
    const char *body = strstr(buffer, "\r\n\r\n");
    if (body) return body + 4; // skip past the \r\n\r\n separator
    return NULL;
}

// ── parse_json_string ──────────────────────────────────────────────────────
// extracts a string value from JSON body for a given key
// e.g. parse_json_string(body, "name", out, 256) finds "name":"VALUE" and copies VALUE
static void parse_json_string(const char *body, const char *key, char *out, int out_size) {
    out[0] = '\0';
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key); // build "key": pattern
    const char *found = strstr(body, search);
    if (!found) return;
    found += strlen(search);
    while (*found == ' ') found++;   // skip whitespace after colon
    if (*found != '"') return;       // value must start with quote
    found++;                          // skip opening quote
    int i = 0;
    while (*found && *found != '"' && i < out_size - 1) {
        out[i++] = *found++;
    }
    out[i] = '\0';
}

// ── parse_json_int ─────────────────────────────────────────────────────────
// extracts an integer value from JSON body for a given key
static int parse_json_int(const char *body, const char *key) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *found = strstr(body, search);
    if (!found) return 0;
    found += strlen(search);
    while (*found == ' ') found++;
    return atoi(found); // atoi converts string to integer
}

// ── router_handle ──────────────────────────────────────────────────────────
void router_handle(int client_fd) {
    char buffer[8192];
    memset(buffer, 0, sizeof(buffer));
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) return;

    char method[8], raw_path[512];
    memset(method, 0, sizeof(method));
    memset(raw_path, 0, sizeof(raw_path));
    sscanf(buffer, "%7s %511s", method, raw_path);

    // split path from query string — e.g. "/api/output?name=X" → path="/api/output"
    // strchr finds the first '?' character in the string
    char path[512];
    strncpy(path, raw_path, sizeof(path));
    char *query = strchr(path, '?');
    if (query) *query++ = '\0'; // terminate path at '?', query now points to "name=X"

    printf("Request: %s %s\n", method, path);

    // ── static file routes ─────────────────────────────────────────────────
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        send_file(client_fd, "web/index.html", "text/html");

    } else if (strcmp(path, "/style.css") == 0) {
        send_file(client_fd, "web/style.css", "text/css");

    } else if (strcmp(path, "/app.js") == 0) {
        send_file(client_fd, "web/app.js", "application/javascript");

    // ── GET /api/projects ──────────────────────────────────────────────────
    } else if (strcmp(path, "/api/projects") == 0 && strcmp(method, "GET") == 0) {
        char json[8192];
        projects_list_json(json, sizeof(json));
        send_json(client_fd, json);

    // ── POST /api/add ──────────────────────────────────────────────────────
    // receives a new project from the browser form and saves it to projects.json
    } else if (strcmp(path, "/api/add") == 0 && strcmp(method, "POST") == 0) {
        const char *body = parse_body(buffer);
        if (!body) { send_json(client_fd, "{\"ok\":false}"); return; }

        Project p;
        memset(&p, 0, sizeof(Project));
        parse_json_string(body, "name",    p.name,    sizeof(p.name));
        parse_json_string(body, "path",    p.path,    sizeof(p.path));
        parse_json_string(body, "command", p.command, sizeof(p.command));
        p.db_port = parse_json_int(body, "db_port");

        if (strlen(p.name) == 0) {
            send_json(client_fd, "{\"ok\":false,\"error\":\"Name is required\"}");
            return;
        }

        int result = projects_add(&p);
        if (result) send_json(client_fd, "{\"ok\":true}");
        else        send_json(client_fd, "{\"ok\":false,\"error\":\"Failed to save\"}");

    // ── POST /api/run ──────────────────────────────────────────────────────
    // spawns the project as a child process
    } else if (strcmp(path, "/api/run") == 0 && strcmp(method, "POST") == 0) {
        const char *body = parse_body(buffer);
        if (!body) { send_json(client_fd, "{\"ok\":false}"); return; }

        char name[256], proj_path[512], command[256];
        parse_json_string(body, "name",    name,      sizeof(name));
        parse_json_string(body, "path",    proj_path, sizeof(proj_path));
        parse_json_string(body, "command", command,   sizeof(command));
        int db_port = parse_json_int(body, "db_port");

        int result = runner_start(name, proj_path, command, db_port);

        if (result == 0) {
            send_json(client_fd, "{\"ok\":true}");
        } else if (result == -2) {
            // db port was not detected — send specific warning
            char warn[256];
            snprintf(warn, sizeof(warn),
                "{\"ok\":false,\"error\":\"Database not detected on port %d. Start it first.\"}",
                db_port);
            send_json(client_fd, warn);
        } else {
            send_json(client_fd, "{\"ok\":false,\"error\":\"Failed to start project\"}");
        }

    // ── GET /api/output ────────────────────────────────────────────────────
    // returns latest output lines from a running project's pipe
    // URL format: /api/output?name=ProjectName
    } else if (strcmp(path, "/api/output") == 0 && strcmp(method, "GET") == 0) {
        char name[256] = {0};

        // extract name from query string e.g. "name=Retro%20Museum"
        // for simplicity we use spaces not URL encoding in project names
        if (query) {
            const char *n = strstr(query, "name=");
            if (n) strncpy(name, n + 5, sizeof(name) - 1);
        }

        char output[4096] = {0};
        int still_running = 0;
        runner_get_output(name, output, sizeof(output), &still_running);

        // build JSON response with output lines and running status
        // we need to escape the output for safe JSON embedding
        // for simplicity we replace newlines with \n in JSON
        char json[8192];
        snprintf(json, sizeof(json),
            "{\"running\":%s,\"output\":\"%s\"}",
            still_running ? "true" : "false",
            output[0] ? output : "");

        send_json(client_fd, json);

    // ── POST /api/stop ─────────────────────────────────────────────────────
    // kills a running project process
    } else if (strcmp(path, "/api/stop") == 0 && strcmp(method, "POST") == 0) {
        const char *body = parse_body(buffer);
        char name[256] = {0};
        if (body) parse_json_string(body, "name", name, sizeof(name));

        int result = runner_stop(name);
        if (result) send_json(client_fd, "{\"ok\":true}");
        else        send_json(client_fd, "{\"ok\":false,\"error\":\"Project not running\"}");

    } else {
        send_404(client_fd);
    }
}

// ── send_json ──────────────────────────────────────────────────────────────
// sends a JSON string response — used by all API endpoints
void send_json(int client_fd, const char *json) {
    char headers[256];
    sprintf(headers,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        strlen(json));
    send(client_fd, headers, strlen(headers), 0);
    send(client_fd, json,    strlen(json),    0);
}

// ── send_file ──────────────────────────────────────────────────────────────
void send_file(int client_fd, const char *filepath, const char *content_type) {
    FILE *f = fopen(filepath, "rb");
    if (!f) { send_404(client_fd); return; }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = (char*)malloc(size + 1);
    if (!content) { fclose(f); send_404(client_fd); return; }

    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    char headers[512];
    sprintf(headers,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n",
        content_type, size);

    send(client_fd, headers,  strlen(headers), 0);
    send(client_fd, content,  size,            0);
    free(content);
}

// ── send_404 ───────────────────────────────────────────────────────────────
void send_404(int client_fd) {
    char *r = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n404 Not Found";
    send(client_fd, r, strlen(r), 0);
}