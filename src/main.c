#include <stdio.h>
#include <windows.h>   // ShellExecute for auto-opening browser
#include "server.h"

int main(void) {
    printf("Sociable Weaver starting...\n");

    // open the browser automatically after a short delay
    // Sleep() pauses for milliseconds — gives the server socket time to bind first
    Sleep(300);
    ShellExecute(NULL, "open", "http://localhost:8080", NULL, NULL, SW_SHOWNORMAL);
    // ShellExecute opens the URL in the default browser
    // NULL = no parent window, "open" = open action, SW_SHOWNORMAL = normal window

    server_start(8080); // never returns — runs the accept loop forever
    return 0;
}