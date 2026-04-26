#include <stdio.h>
#include "server.h"

int main() {
    printf("Sociable Weaver is starting...\n");
    server_start(8080);
    return 0;
}