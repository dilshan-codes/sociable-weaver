#include <winsock2.h>
// Needed for send() and recv() — the functions that read and write data through a socket connection.
#include <stdio.h>
#include <string.h>
// Needed for strlen() and memset().
// strlen() counts how many characters are in a string. memset() fills memory with a specific value — we use it to zero buffers.
#include <stdlib.h>
// Needed for malloc() and free() — allocate and release heap memory for file contents.
#include "router.h"
#include "projects.h"

// ── forward declarations ───────────────────────────────────────────────────
// These tell the compiler these functions exist later in this same file.
// Without them, router_handle() cannot call send_file() or send_404()
// because they are defined below router_handle().
void send_file(int client_fd, const char *filepath, const char *content_type);
void send_404(int client_fd);

void router_handle(int client_fd) {
// Called by server.c every time a browser sends a request.
// Parameter: client_fd = the integer ID of this browser connection. We use client_fd to receive data from the browser and send data back.

    char buffer[8192];
    // Increased from 4096 to 8192 — Day 2 requests include more headers.
    // When a browser visits localhost:8080 it sends a block of text through the socket. That text is the HTTP request. We need a place to store those incoming bytes — that is the buffer.

    memset(buffer, 0, sizeof(buffer));
    // memset() fills every byte of buffer with the value 0. Arguments:
    // &buffer = address of the buffer (where to start filling)
    // 0 = the value to fill with
    // sizeof(buffer) = how many bytes to fill

    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    // recv() reads incoming data from the browser connection into buffer. recv() returns how many bytes were actually received.
    // Arguments:
    // client_fd = which connection to read from
    // buffer = where to store the received bytes
    // sizeof(buffer) - 1 = maximum bytes to read
    // 0 = no special flags

    if (bytes_received <= 0) return;
    // If recv() returns 0 or negative, the browser disconnected — nothing to do

    char method[8];
    char path[512];
    // The first line of an HTTP request always looks like this: GET /index.html HTTP/1.1
    // We need to extract two pieces: the method and the path.
    // char method[8] = array to hold the method (GET, POST, etc.)
    // 8 characters is enough for any HTTP method.
    // char path[512] = array to hold the path (/index.html, /api/projects, etc.)

    memset(method, 0, sizeof(method));
    memset(path, 0, sizeof(path));
    // memset both arrays to zero before using them — same reason as buffer above

    sscanf(buffer, "%7s %511s", method, path);
    // sscanf() reads formatted data from a string — like scanf() but from
    // a string in memory instead of from keyboard input.
    // "%7s %511s" means:
    //   %7s = read up to 7 characters into method (leaves room for \0)
    //   (space) = skip any whitespace between the two values
    //   %511s = read up to 511 characters into path (leaves room for \0)

    printf("Request: %s %s\n", method, path);
    // Log the request to the terminal so we can see what is happening, %s in printf is replaced with the string value of method and path

    // ── route each path to the correct file ───────────────────────────────
    // strcmp() compares two strings — returns 0 if they are equal.
    // We check the path the browser asked for and call send_file() with
    // the matching file on disk and the correct Content-Type for that file.
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        send_file(client_fd, "web/index.html", "text/html"); // serve the dashboard page

    } else if (strcmp(path, "/style.css") == 0) {
        send_file(client_fd, "web/style.css", "text/css"); // serve the stylesheet

    } else if (strcmp(path, "/app.js") == 0) {
        send_file(client_fd, "web/app.js", "application/javascript"); // serve the JS
    
    } else if (strcmp(path, "/api/projects") == 0) {
    // browser is asking for the project list as JSON
    char json[8192];
    projects_list_json(json, sizeof(json)); // fills json with the array string

    char headers[256];
    sprintf(headers,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n" // tells browser this is JSON not HTML
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        strlen(json));

    send(client_fd, headers, strlen(headers), 0);
    send(client_fd, json, strlen(json), 0);

    } else {
        send_404(client_fd); // path not recognised — send a 404 response
    }
}

// ── send_file ──────────────────────────────────────────────────────────────
// reads a file from disk and sends it to the browser with correct HTTP headers.
// Parameters:
// client_fd = the browser connection to send to
// filepath = path to the file on disk e.g. "web/index.html"
// content_type = the MIME type to tell the browser e.g. "text/html"
void send_file(int client_fd, const char *filepath, const char *content_type) {
    FILE *f = fopen(filepath, "rb"); // fopen opens the file. "rb" = read binary mode
    if (!f) {
        send_404(client_fd); // file not found on disk — send 404
        return;
    }

    fseek(f, 0, SEEK_END);  // move file cursor to the end
    long size = ftell(f);   // ftell returns current position = file size in bytes
    fseek(f, 0, SEEK_SET);  // move cursor back to the start before reading

    char *content = (char*)malloc(size + 1);
    // malloc() allocates 'size' bytes on the heap at runtime.
    // We use heap here because file size is unknown at compile time —
    // stack arrays need a fixed size known before the program runs.
    // +1 = extra byte for the null terminator at the end of the string.
    if (!content) { fclose(f); send_404(client_fd); return; }
    // if malloc failed (out of memory) — clean up and send 404

    fread(content, 1, size, f); // read entire file into content buffer
    // Arguments: content = destination, 1 = read 1 byte at a time, size = how many, f = source file
    content[size] = '\0'; // null terminate so string functions work correctly
    fclose(f); // always close the file when done reading

    // build HTTP response headers
    char headers[512];
    sprintf(headers,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"       // tells browser what kind of file this is
        "Content-Length: %ld\r\n"    // tells browser exactly how many bytes are coming
        "Connection: close\r\n"
        "\r\n",                       // blank line required — separates headers from body
        content_type, size);

    send(client_fd, headers, strlen(headers), 0); // send headers first
    send(client_fd, content, size, 0);             // then send the file content as the body

    free(content); // always free malloc'd memory when done — forgetting causes memory leaks
}

// ── send_404 ───────────────────────────────────────────────────────────────
// sends a 404 Not Found response when the browser asks for a path we do not have
void send_404(int client_fd) {
    char *response =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n"
        "404 — Page not found";
    send(client_fd, response, strlen(response), 0);
}