#include <winsock2.h>
// Needed for send() and recv() — the functions that read and write data through a socket connection.
#include <stdio.h>
#include <string.h>
// Needed for strlen() and memset().
// strlen() counts how many characters are in a string. memset() fills memory with a specific value — we use it to zero buffers.
#include "router.h"

void router_handle(int client_fd) {
// Called by server.c every time a browser sends a request.
// Parameter: client_fd = the integer ID of this browser connection. We use client_fd to receive data from the browser and send data back.

    char buffer[4096];
    // When a browser visits localhost:8080 it sends a block of text through the socket. That text is the HTTP request. We need a place to store those incoming bytes — that is the buffer.
    // char buffer[4096] declares an array of 4096 characters on the stack. this array is created when the function starts and destroyed when it ends. 4096 bytes = 4KB which is enough for any typical HTTP request header.

    memset(buffer, 0, sizeof(buffer));
    // memset() fills every byte of buffer with the value 0. Arguments:
    // &buffer = address of the buffer (where to start filling)
    // 0 = the value to fill with
    // sizeof(buffer) = how many bytes to fill (4096 in this case)

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

    
    // An HTTP response has a specific structure:
    // Line 1: Status line — HTTP version, status code, reason phrase
    // Line 2+: Headers — key: value pairs giving info about the response
    // Blank line: Required separator between headers and body
    // Body: The actual content the browser displays
    //
    // \r\n = carriage return + newline.
    // HTTP requires \r\n at the end of each header line, not just \n.
    // Using only \n would violate the HTTP specification and some
    // browsers would reject or misinterpret the response.

    char *response =
        "HTTP/1.1 200 OK\r\n"
        // HTTP/1.1 = the HTTP version we are speaking
        // 200 = status code meaning success
        // OK = human readable reason for the status code

        "Content-Type: text/plain\r\n"
        // Content-Type tells the browser what kind of data is coming.
        // text/plain = plain text, display it as-is.
        // On Day 2 this becomes text/html for HTML files, text/css for CSS etc.

        "Connection: close\r\n"
        // Connection: close tells the browser we will close the connection
        // after sending this response. This keeps our server simple for now.

        "\r\n"
        // This blank line is REQUIRED by the HTTP specification.
        // It separates the headers above from the body below.
        // Without it the browser treats everything as headers and displays nothing.

        "Sociable Weaver is alive.";
        // This is the body — what the browser actually displays.

    send(client_fd, response, strlen(response), 0);
    // send() pushes bytes from our response string through the socket to the browser. Arguments:
    // client_fd = which connection to send to
    // response = the data to send
    // strlen(response) = how many bytes to send
    //   strlen() counts characters until it hits the null terminator \0
    // 0 = no special flags
}